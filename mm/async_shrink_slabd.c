#include <linux/mm.h>
#include <linux/sched/mm.h>
#include <linux/module.h>
#include <linux/gfp.h>
#include <linux/kernel_stat.h>
#include <linux/swap.h>
#include <linux/pagemap.h>
#include <linux/init.h>
#include <linux/highmem.h>
#include <linux/vmpressure.h>
#include <linux/vmstat.h>
#include <linux/file.h>
#include <linux/writeback.h>
#include <linux/blkdev.h>
#include <linux/mm_inline.h>
#include <linux/backing-dev.h>
#include <linux/rmap.h>
#include <linux/topology.h>
#include <linux/cpu.h>
#include <linux/cpuset.h>
#include <linux/compaction.h>
#include <linux/notifier.h>
#include <linux/rwsem.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/memcontrol.h>
#include <linux/delayacct.h>
#include <linux/sysctl.h>
#include <linux/oom.h>
#include <linux/prefetch.h>
#include <linux/printk.h>
#include <linux/dax.h>
#include <linux/psi.h>
#include <linux/swapops.h>
#include <linux/cpufreq.h>

#if defined(OPLUS_FEATURE_PROCESS_RECLAIM) && defined(CONFIG_PROCESS_RECLAIM_ENHANCE)
#include <linux/process_mm_reclaim.h>
#endif

#include "internal.h"

extern unsigned long shrink_slab(gfp_t gfp_mask, int nid,
				 struct mem_cgroup *memcg,
				 int priority);

static bool async_shrink_slabd_setup;
static DECLARE_WAIT_QUEUE_HEAD(shrink_slabd_wait);
static DEFINE_SPINLOCK(shrink_slabd_lock);

/* One request is owned by the worker until its memcg reference is dropped. */
static struct async_slabd_parameter {
	gfp_t gfp_mask;
	int nid;
	struct mem_cgroup *memcg;
	int priority;
	bool busy;
} asp;

static struct task_struct *shrink_slabd_tsk;
/* Completed global-reclaim slab pages, never shrinker object counts. */
static atomic_long_t completed_slab_pages[MAX_NUMNODES];

unsigned long async_shrink_slabd_reclaimed(int nid)
{
	return atomic_long_xchg(&completed_slab_pages[nid], 0);
}

bool wakeup_shrink_slabd(gfp_t gfp_mask, int nid,
			 struct mem_cgroup *memcg, int priority)
{
	unsigned long flags;

	/* Pairs with init's release of the worker pointer. */
	if (unlikely(!smp_load_acquire(&async_shrink_slabd_setup)))
		return false;

	spin_lock_irqsave(&shrink_slabd_lock, flags);
	if (asp.busy) {
		spin_unlock_irqrestore(&shrink_slabd_lock, flags);
		return false;
	}
#ifdef CONFIG_MEMCG
	/* The iterator's reference may disappear as soon as we return. */
	if (memcg)
		css_get(&memcg->css);
#endif
	asp.gfp_mask = gfp_mask;
	asp.nid = nid;
	asp.memcg = memcg;
	asp.priority = priority;
	WRITE_ONCE(asp.busy, true);
	spin_unlock_irqrestore(&shrink_slabd_lock, flags);
	wake_up_interruptible(&shrink_slabd_wait);

	return true;
}

void set_async_slabd_cpus(void)
{
	struct cpumask mask;
	unsigned int cpu, max_freq = 0;
	static bool set_slabd_cpus_success;

	/* Pairs with init's release of the worker pointer. */
	if (unlikely(!smp_load_acquire(&async_shrink_slabd_setup)))
		return;

	if (likely(set_slabd_cpus_success))
		return;

	cpumask_clear(&mask);
	for_each_possible_cpu(cpu) {
		struct cpufreq_policy *policy = cpufreq_cpu_get(cpu);

		if (!policy)
			continue;

		if (policy->cpuinfo.max_freq >= max_freq) {
			max_freq = policy->cpuinfo.max_freq;
			cpumask_andnot(&mask, cpumask_of_node(0),
				       policy->related_cpus);
		}
		cpufreq_cpu_put(policy);
	}

	if (!cpumask_empty(&mask) &&
	    !set_cpus_allowed_ptr(shrink_slabd_tsk, &mask))
		set_slabd_cpus_success = true;
}

static int kshrink_slabd_func(void *p)
{
	int nid, priority;
	struct mem_cgroup *memcg;
	gfp_t gfp_mask;
	/*
	 * Tell the memory management that we're a "memory allocator",
	 * and that if we need more memory we should get access to it
	 * regardless (see "__alloc_pages()"). "kswapd" should
	 * never get caught in the normal page freeing logic.
	 *
	 * (Kswapd normally doesn't need memory anyway, but sometimes
	 * you need a small amount of memory in order to be able to
	 * page out something else, and this flag essentially protects
	 * us from recursively trying to free more memory as we're
	 * trying to free the first piece of memory in the first place).
	 */
	current->flags |= PF_MEMALLOC | PF_SWAPWRITE | PF_KSWAPD;
	set_freezable();

	for (;;) {
		struct reclaim_state reclaim_state = {};
		unsigned long flags;

		wait_event_freezable(shrink_slabd_wait,
				     READ_ONCE(asp.busy) || kthread_should_stop());
		spin_lock_irqsave(&shrink_slabd_lock, flags);
		if (!asp.busy) {
			spin_unlock_irqrestore(&shrink_slabd_lock, flags);
			if (kthread_should_stop())
				break;
			continue;
		}
		gfp_mask = asp.gfp_mask;
		nid = asp.nid;
		priority = asp.priority;
		memcg = asp.memcg;
		spin_unlock_irqrestore(&shrink_slabd_lock, flags);

		set_async_slabd_cpus();
		current->reclaim_state = &reclaim_state;
		shrink_slab(gfp_mask, nid, memcg, priority);
		current->reclaim_state = NULL;

		/* SLAB/SLUB account freed pages in reclaim_state, not objects. */
		atomic_long_add(reclaim_state.reclaimed_slab,
				&completed_slab_pages[nid]);
		mem_cgroup_put(memcg);

		spin_lock_irqsave(&shrink_slabd_lock, flags);
		WRITE_ONCE(asp.busy, false);
		spin_unlock_irqrestore(&shrink_slabd_lock, flags);
	}
	current->flags &= ~(PF_MEMALLOC | PF_SWAPWRITE | PF_KSWAPD);
	current->reclaim_state = NULL;

	return 0;
}

static int async_shrink_slabd_init(void)
{
	int ret;

	shrink_slabd_tsk = kthread_run(kshrink_slabd_func, NULL, "kshrink_slabd");
	if (IS_ERR_OR_NULL(shrink_slabd_tsk)) {
		pr_err("Failed to start shrink_slabd on node 0\n");
		ret = PTR_ERR(shrink_slabd_tsk);
		shrink_slabd_tsk = NULL;
		return ret;
	}

	/* Publish the worker pointer before allowing requests. */
	smp_store_release(&async_shrink_slabd_setup, true);
	return 0;
}
module_init(async_shrink_slabd_init);
