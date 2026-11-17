// SPDX-License-Identifier: GPL-2.0
/*
 * DAMON Operations for The Physical Address Space
 *
 * Author: SeongJae Park <sj@kernel.org>
 */

#define pr_fmt(fmt) "damon-pa: " fmt

#include <linux/mmu_notifier.h>
#include <linux/page_idle.h>
#include <linux/pagemap.h>
#include <linux/rmap.h>

#include "../internal.h"
#include "ops-common.h"

static bool __damon_pa_mkold(struct page *page, struct vm_area_struct *vma,
		unsigned long addr, void *arg)
{
	struct page_vma_mapped_walk pvmw = {
		.page = page,
		.vma = vma,
		.address = addr,
	};

	while (page_vma_mapped_walk(&pvmw)) {
		addr = pvmw.address;
		if (pvmw.pte)
			damon_ptep_mkold(pvmw.pte, vma, addr);
		else
			damon_pmdp_mkold(pvmw.pmd, vma, addr);
	}
	return true;
}

static void damon_pa_mkold(unsigned long paddr)
{
	struct page *page = damon_get_page(PHYS_PFN(paddr));
	struct rmap_walk_control rwc = {
		.rmap_one = __damon_pa_mkold,
		.anon_lock = page_lock_anon_vma_read,
	};
	bool need_lock;

	if (!page)
		return;

	if (!page_mapped(page) || !page_rmapping(page)) {
		set_page_idle(page);
		goto out;
	}

	need_lock = !PageAnon(page) || PageKsm(page);
	if (need_lock && !trylock_page(page))
		goto out;

	rmap_walk(page, &rwc);

	if (need_lock)
		unlock_page(page);

out:
	put_page(page);
}

static void __damon_pa_prepare_access_check(struct damon_ctx *ctx,
					    struct damon_region *r)
{
	r->sampling_addr = damon_rand(r->ar.start, r->ar.end);

	damon_pa_mkold(r->sampling_addr);
}

static void damon_pa_prepare_access_checks(struct damon_ctx *ctx)
{
	struct damon_target *t;
	struct damon_region *r;

	damon_for_each_target(t, ctx) {
		damon_for_each_region(r, t)
			__damon_pa_prepare_access_check(ctx, r);
	}
}

struct damon_pa_access_chk_result {
	unsigned long page_sz;
	bool accessed;
};

static bool __damon_pa_young(struct page *page, struct vm_area_struct *vma,
		unsigned long addr, void *arg)
{
	struct damon_pa_access_chk_result *result = arg;
	struct page_vma_mapped_walk pvmw = {
		.page = page,
		.vma = vma,
		.address = addr,
	};

	result->accessed = false;
	result->page_sz = PAGE_SIZE;
	while (page_vma_mapped_walk(&pvmw)) {
		addr = pvmw.address;
		if (pvmw.pte) {
			result->accessed = pte_young(*pvmw.pte) ||
				!page_is_idle(page) ||
				mmu_notifier_test_young(vma->vm_mm, addr);
		} else {
#ifdef CONFIG_TRANSPARENT_HUGEPAGE
			result->accessed = pmd_young(*pvmw.pmd) ||
				!page_is_idle(page) ||
				mmu_notifier_test_young(vma->vm_mm, addr);
			result->page_sz = ((1UL) << HPAGE_PMD_SHIFT);
#else
			WARN_ON_ONCE(1);
#endif	/* CONFIG_TRANSPARENT_HUGEPAGE */
		}
		if (result->accessed) {
			page_vma_mapped_walk_done(&pvmw);
			break;
		}
	}

	/* If accessed, stop walking */
	return !result->accessed;
}

static bool damon_pa_young(unsigned long paddr, unsigned long *page_sz)
{
	struct page *page = damon_get_page(PHYS_PFN(paddr));
	struct damon_pa_access_chk_result result = {
		.page_sz = PAGE_SIZE,
		.accessed = false,
	};
	struct rmap_walk_control rwc = {
		.arg = &result,
		.rmap_one = __damon_pa_young,
		.anon_lock = page_lock_anon_vma_read,
	};
	bool need_lock;

	if (!page)
		return false;

	if (!page_mapped(page) || !page_rmapping(page)) {
		if (page_is_idle(page))
			result.accessed = false;
		else
			result.accessed = true;
		put_page(page);
		goto out;
	}

	need_lock = !PageAnon(page) || PageKsm(page);
	if (need_lock && !trylock_page(page)) {
		put_page(page);
		return false;
	}

	rmap_walk(page, &rwc);

	if (need_lock)
		unlock_page(page);
	put_page(page);

out:
	*page_sz = result.page_sz;
	return result.accessed;
}

struct damon_pa_access_chk_cache {
	unsigned long addr;
	unsigned long page_sz;
	bool accessed;
	bool valid;
};

static void __damon_pa_check_access(struct damon_region *r,
				  struct damon_pa_access_chk_cache *cache)
{
	/* Reuse a page's result only within this sampling round. */
	if (!cache->valid ||
	    ALIGN_DOWN(cache->addr, cache->page_sz) !=
	    ALIGN_DOWN(r->sampling_addr, cache->page_sz)) {
		cache->page_sz = PAGE_SIZE;
		cache->accessed = damon_pa_young(r->sampling_addr,
					       &cache->page_sz);
		cache->addr = r->sampling_addr;
		cache->valid = true;
	}
	if (cache->accessed)
		r->nr_accesses++;
}

static unsigned int damon_pa_check_accesses(struct damon_ctx *ctx)
{
	struct damon_pa_access_chk_cache cache = {};
	struct damon_target *t;
	struct damon_region *r;
	unsigned int max_nr_accesses = 0;

	damon_for_each_target(t, ctx) {
		damon_for_each_region(r, t) {
			__damon_pa_check_access(r, &cache);
			max_nr_accesses = max(r->nr_accesses, max_nr_accesses);
		}
	}

	return max_nr_accesses;
}

static unsigned long damon_pa_apply_scheme(struct damon_ctx *ctx,
				struct damon_target *t, struct damon_region *r,
				struct damos *scheme)
{
	unsigned long pfn = PHYS_PFN(r->ar.start);
	unsigned long end_pfn = PHYS_PFN(r->ar.end);
	unsigned long applied;
	LIST_HEAD(page_list);

	if (scheme->action != DAMOS_PAGEOUT)
		return 0;

	/* Only reclaim pages entirely contained in the requested region. */
	if (!IS_ALIGNED(r->ar.start, PAGE_SIZE))
		pfn++;
	for (; pfn < end_pfn; pfn++) {
		struct page *page = damon_get_page(pfn);
		unsigned long nr_pages;

		if (!page)
			continue;

		nr_pages = hpage_nr_pages(page);
		if (nr_pages > end_pfn - pfn) {
			put_page(page);
			continue;
		}
		/* Skip compound tails even if reclaim splits the isolated page. */
		pfn += nr_pages - 1;

		ClearPageReferenced(page);
		test_and_clear_page_young(page);
		if (isolate_lru_page(page)) {
			put_page(page);
			continue;
		}
		if (PageUnevictable(page))
			putback_lru_page(page);
		else
			list_add(&page->lru, &page_list);
		/* Drop the lookup reference; reclaim/putback owns the isolate pin. */
		put_page(page);
	}
	applied = reclaim_pages(&page_list);
	cond_resched();
	pr_debug("reclaimed %lu pages\n", applied);
	nr_reclaim_page = nr_reclaim_page + applied;
	return applied * PAGE_SIZE;
}

static int damon_pa_scheme_score(struct damon_ctx *context,
        		struct damon_target *t, struct damon_region *r,
				struct damos *scheme)
{
	switch (scheme->action) {
	case DAMOS_PAGEOUT:
		return damon_pageout_score(context, r, scheme);
	default:
		break;
	}

	return DAMOS_MAX_SCORE;
}

static int __init damon_pa_initcall(void)
{
	struct damon_operations ops = {
		.id = DAMON_OPS_PADDR,
		.init = NULL,
		.update = NULL,
		.prepare_access_checks = damon_pa_prepare_access_checks,
		.check_accesses = damon_pa_check_accesses,
		.reset_aggregated = NULL,
		.target_valid = NULL,
		.cleanup = NULL,
		.apply_scheme = damon_pa_apply_scheme,
		.get_scheme_score = damon_pa_scheme_score,
	};

	return damon_register_ops(&ops);
};

subsys_initcall(damon_pa_initcall);
