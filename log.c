#ifdef CONFIG_DEBUG_MUTEXES

/**
 * mutex_init - initialize the mutex
 * @mutex: the mutex to be initialized
 *
 * Initialize the mutex to unlocked state.
 *
 * It is not allowed to initialize an already locked mutex.
 */
#define mutex_init(mutex)						\
do {									\
	static struct lock_class_key __key;				\
									\
	__mutex_init((mutex), #mutex, &__key);				\
} while (0)

void
__mutex_init(struct mutex *lock, const char *name, struct lock_class_key *key)
{
	atomic_long_set(&lock->owner, 0);
	raw_spin_lock_init(&lock->wait_lock);
	INIT_LIST_HEAD(&lock->wait_list);
#ifdef CONFIG_MUTEX_SPIN_ON_OWNER
	osq_lock_init(&lock->osq);
#endif

	debug_mutex_init(lock, name, key);
}
EXPORT_SYMBOL(__mutex_init);

#ifdef CONFIG_DEBUG_MUTEXES

void debug_mutex_init(struct mutex *lock, const char *name,
		      struct lock_class_key *key)
{
#ifdef CONFIG_DEBUG_LOCK_ALLOC
	/*
	 * Make sure we are not reinitializing a held lock:
	 */
	debug_check_no_locks_freed((void *)lock, sizeof(*lock));
	lockdep_init_map_wait(&lock->dep_map, name, key, 0, LD_WAIT_SLEEP);
#endif
	lock->magic = lock;
}

#endif

#define init_rwsem(sem)						\
do {								\
	static struct lock_class_key __key;			\
								\
	__init_rwsem((sem), #sem, &__key);			\
} while (0)

/*
 * Initialize an rwsem:
 */
void __init_rwsem(struct rw_semaphore *sem, const char *name,
		  struct lock_class_key *key)
{
#ifdef CONFIG_DEBUG_LOCK_ALLOC
	/*
	 * Make sure we are not reinitializing a held semaphore:
	 */
	debug_check_no_locks_freed((void *)sem, sizeof(*sem));
	lockdep_init_map_wait(&sem->dep_map, name, key, 0, LD_WAIT_SLEEP);
#endif
#ifdef CONFIG_DEBUG_RWSEMS
	sem->magic = sem;
#endif
	atomic_long_set(&sem->count, RWSEM_UNLOCKED_VALUE);
	raw_spin_lock_init(&sem->wait_lock);
	INIT_LIST_HEAD(&sem->wait_list);
	atomic_long_set(&sem->owner, 0L);
#ifdef CONFIG_RWSEM_SPIN_ON_OWNER
	osq_lock_init(&sem->osq);
#endif
}
EXPORT_SYMBOL(__init_rwsem);

#ifdef CONFIG_DEBUG_SPINLOCK

# define spin_lock_init(lock)					\
do {								\
	static struct lock_class_key __key;			\
								\
	__raw_spin_lock_init(spinlock_check(lock),		\
			     #lock, &__key, LD_WAIT_CONFIG);	\
} while (0)

void __raw_spin_lock_init(raw_spinlock_t *lock, const char *name,
			  struct lock_class_key *key, short inner)
{
#ifdef CONFIG_DEBUG_LOCK_ALLOC
	/*
	 * Make sure we are not reinitializing a held lock:
	 */
	debug_check_no_locks_freed((void *)lock, sizeof(*lock));
	lockdep_init_map_wait(&lock->dep_map, name, key, 0, inner);
#endif
	lock->raw_lock = (arch_spinlock_t)__ARCH_SPIN_LOCK_UNLOCKED;
	lock->magic = SPINLOCK_MAGIC;
	lock->owner = SPINLOCK_OWNER_INIT;
	lock->owner_cpu = -1;
}

#endif

static __always_inline void spin_lock(spinlock_t *lock)
{
	raw_spin_lock(&lock->rlock);
}

#define raw_spin_lock(lock)	_raw_spin_lock(lock)

#ifdef CONFIG_INLINE_SPIN_LOCK
#define _raw_spin_lock(lock) __raw_spin_lock(lock)
#else
void __lockfunc _raw_spin_lock(raw_spinlock_t *lock)
{
	__raw_spin_lock(lock);
}
EXPORT_SYMBOL(_raw_spin_lock);
#endif

/*
 * If lockdep is enabled then we use the non-preemption spin-ops
 * even on CONFIG_PREEMPTION, because lockdep assumes that interrupts are
 * not re-enabled during lock-acquire (which the preempt-spin-ops do):
 */
#if !defined(CONFIG_GENERIC_LOCKBREAK) || defined(CONFIG_DEBUG_LOCK_ALLOC)
static inline void __raw_spin_lock_irq(raw_spinlock_t *lock)
{
	local_irq_disable();
	preempt_disable();
	spin_acquire(&lock->dep_map, 0, 0, _RET_IP_);
	LOCK_CONTENDED(lock, do_raw_spin_trylock, do_raw_spin_lock);
}
#endif

#define raw_spin_unlock(lock)		_raw_spin_unlock(lock)
#ifdef CONFIG_UNINLINE_SPIN_UNLOCK
void __lockfunc _raw_spin_unlock(raw_spinlock_t *lock)
{
	__raw_spin_unlock(lock);
}
EXPORT_SYMBOL(_raw_spin_unlock);
#endif

static inline void __raw_spin_unlock(raw_spinlock_t *lock)
{
	spin_release(&lock->dep_map, _RET_IP_);
	do_raw_spin_unlock(lock);
	preempt_enable();
}



#define percpu_init_rwsem(sem)					\
({								\
	static struct lock_class_key rwsem_key;			\
	__percpu_init_rwsem(sem, #sem, &rwsem_key);		\
})

int __percpu_init_rwsem(struct percpu_rw_semaphore *sem,
			const char *name, struct lock_class_key *key)
{
	sem->read_count = alloc_percpu(int);
	if (unlikely(!sem->read_count))
		return -ENOMEM;

	rcu_sync_init(&sem->rss);
	rcuwait_init(&sem->writer);
	init_waitqueue_head(&sem->waiters);
	atomic_set(&sem->block, 0);
#ifdef CONFIG_DEBUG_LOCK_ALLOC
	debug_check_no_locks_freed((void *)sem, sizeof(*sem));
	lockdep_init_map(&sem->dep_map, name, key, 0);
#endif
	return 0;
}
EXPORT_SYMBOL_GPL(__percpu_init_rwsem);

#ifdef CONFIG_LOCKDEP

static __always_inline bool lockdep_enabled(void)
{
	if (!debug_locks)
		return false;

	if (this_cpu_read(lockdep_recursion))
		return false;

	if (current->lockdep_recursion)
		return false;

	return true;
}

static inline void
lockdep_init_map_waits(struct lockdep_map *lock, const char *name,
		       struct lock_class_key *key, int subclass, u8 inner, u8 outer)
{
	lockdep_init_map_type(lock, name, key, subclass, inner, LD_WAIT_INV, LD_LOCK_NORMAL);
}

static inline void
lockdep_init_map_wait(struct lockdep_map *lock, const char *name,
		      struct lock_class_key *key, int subclass, u8 inner)
{
	lockdep_init_map_waits(lock, name, key, subclass, inner, LD_WAIT_INV);
}

static inline void lockdep_init_map(struct lockdep_map *lock, const char *name,
			     struct lock_class_key *key, int subclass)
{
	lockdep_init_map_wait(lock, name, key, subclass, LD_WAIT_INV);
}

/*
 * Initialize a lock instance's lock-class mapping info:
 */
void lockdep_init_map_type(struct lockdep_map *lock, const char *name,
			    struct lock_class_key *key, int subclass,
			    u8 inner, u8 outer, u8 lock_type)
{
	int i;

	for (i = 0; i < NR_LOCKDEP_CACHING_CLASSES; i++)
		lock->class_cache[i] = NULL;

#ifdef CONFIG_LOCK_STAT
	lock->cpu = raw_smp_processor_id();
#endif

	/*
	 * Can't be having no nameless bastards around this place!
	 */
	if (DEBUG_LOCKS_WARN_ON(!name)) {
		lock->name = "NULL";
		return;
	}

	lock->name = name;

	lock->wait_type_outer = outer;
	lock->wait_type_inner = inner;
	lock->lock_type = lock_type;

	/*
	 * No key, no joy, we need to hash something.
	 */
	if (DEBUG_LOCKS_WARN_ON(!key))
		return;
	/*
	 * Sanity check, the lock-class key must either have been allocated
	 * statically or must have been registered as a dynamic key.
	 */
	if (!static_obj(key) && !is_dynamic_key(key)) {
		if (debug_locks)
			printk(KERN_ERR "BUG: key %px has not been registered!\n", key);
		DEBUG_LOCKS_WARN_ON(1);
		return;
	}
	lock->key = key;

	if (unlikely(!debug_locks))
		return;

	if (subclass) {
		unsigned long flags;

		if (DEBUG_LOCKS_WARN_ON(!lockdep_enabled()))
			return;

		raw_local_irq_save(flags);
		lockdep_recursion_inc();
		register_lock_class(lock, subclass, 1);
		lockdep_recursion_finish();
		raw_local_irq_restore(flags);
	}
}
EXPORT_SYMBOL_GPL(lockdep_init_map_type);

/*
 * We are not always called with irqs disabled - do that here,
 * and also avoid lockdep recursion:
 */
void lock_acquire(struct lockdep_map *lock, unsigned int subclass,
			  int trylock, int read, int check,
			  struct lockdep_map *nest_lock, unsigned long ip)
{
	unsigned long flags;

	trace_lock_acquire(lock, subclass, trylock, read, check, nest_lock, ip);

	if (!debug_locks)
		return;

	if (unlikely(!lockdep_enabled())) {
		/* XXX allow trylock from NMI ?!? */
		if (lockdep_nmi() && !trylock) {
			struct held_lock hlock;

			hlock.acquire_ip = ip;
			hlock.instance = lock;
			hlock.nest_lock = nest_lock;
			hlock.irq_context = 2; // XXX
			hlock.trylock = trylock;
			hlock.read = read;
			hlock.check = check;
			hlock.hardirqs_off = true;
			hlock.references = 0;

			verify_lock_unused(lock, &hlock, subclass);
		}
		return;
	}

	raw_local_irq_save(flags);
	check_flags(flags);

	lockdep_recursion_inc();
	__lock_acquire(lock, subclass, trylock, read, check,
		       irqs_disabled_flags(flags), nest_lock, ip, 0, 0);
	lockdep_recursion_finish();
	raw_local_irq_restore(flags);
}
EXPORT_SYMBOL_GPL(lock_acquire);

void lock_contended(struct lockdep_map *lock, unsigned long ip)
{
	unsigned long flags;

	trace_lock_contended(lock, ip);

	if (unlikely(!lock_stat || !lockdep_enabled()))
		return;

	raw_local_irq_save(flags);
	check_flags(flags);
	lockdep_recursion_inc();
	__lock_contended(lock, ip);
	lockdep_recursion_finish();
	raw_local_irq_restore(flags);
}
EXPORT_SYMBOL_GPL(lock_contended);

void lock_acquired(struct lockdep_map *lock, unsigned long ip)
{
	unsigned long flags;

	trace_lock_acquired(lock, ip);

	if (unlikely(!lock_stat || !lockdep_enabled()))
		return;

	raw_local_irq_save(flags);
	check_flags(flags);
	lockdep_recursion_inc();
	__lock_acquired(lock, ip);
	lockdep_recursion_finish();
	raw_local_irq_restore(flags);
}
EXPORT_SYMBOL_GPL(lock_acquired);

void lock_release(struct lockdep_map *lock, unsigned long ip)
{
	unsigned long flags;

	trace_lock_release(lock, ip);

	if (unlikely(!lockdep_enabled()))
		return;

	raw_local_irq_save(flags);
	check_flags(flags);

	lockdep_recursion_inc();
	if (__lock_release(lock, ip))
		check_chain_key(current);
	lockdep_recursion_finish();
	raw_local_irq_restore(flags);
}
EXPORT_SYMBOL_GPL(lock_release);

TRACE_EVENT(lock_acquire,

	TP_PROTO(struct lockdep_map *lock, unsigned int subclass,
		int trylock, int read, int check,
		struct lockdep_map *next_lock, unsigned long ip),

	TP_ARGS(lock, subclass, trylock, read, check, next_lock, ip),

	TP_STRUCT__entry(
		__field(unsigned int, flags)
		__string(name, lock->name)
		__field(void *, lockdep_addr)
	),

	TP_fast_assign(
		__entry->flags = (trylock ? 1 : 0) | (read ? 2 : 0);
		__assign_str(name, lock->name);
		__entry->lockdep_addr = lock;
	),

	TP_printk("%p %s%s%s", __entry->lockdep_addr,
		  (__entry->flags & 1) ? "try " : "",
		  (__entry->flags & 2) ? "read " : "",
		  __get_str(name))
);

DECLARE_EVENT_CLASS(lock,

	TP_PROTO(struct lockdep_map *lock, unsigned long ip),

	TP_ARGS(lock, ip),

	TP_STRUCT__entry(
		__string(	name, 	lock->name	)
		__field(	void *, lockdep_addr	)
	),

	TP_fast_assign(
		__assign_str(name, lock->name);
		__entry->lockdep_addr = lock;
	),

	TP_printk("%p %s",  __entry->lockdep_addr, __get_str(name))
);

DEFINE_EVENT(lock, lock_release,

	TP_PROTO(struct lockdep_map *lock, unsigned long ip),

	TP_ARGS(lock, ip)
);

#endif

#define spin_acquire(l, s, t, i)		lock_acquire_exclusive(l, s, t, NULL, i)
#define lock_acquire_exclusive(l, s, t, n, i)		lock_acquire(l, s, t, 0, 1, n, i)
#define spin_release(l, i)			lock_release(l, i)

#ifdef CONFIG_LOCK_STAT

DEFINE_EVENT(lock, lock_contended,

	TP_PROTO(struct lockdep_map *lock, unsigned long ip),

	TP_ARGS(lock, ip)
);

DEFINE_EVENT(lock, lock_acquired,

	TP_PROTO(struct lockdep_map *lock, unsigned long ip),

	TP_ARGS(lock, ip)
);

#define LOCK_CONTENDED(_lock, try, lock)			\
do {								\
	if (!try(_lock)) {					\
		lock_contended(&(_lock)->dep_map, _RET_IP_);	\
		lock(_lock);					\
	}							\
	lock_acquired(&(_lock)->dep_map, _RET_IP_);			\
} while (0)

#define LOCK_CONTENDED_RETURN(_lock, try, lock)			\
({								\
	int ____err = 0;					\
	if (!try(_lock)) {					\
		lock_contended(&(_lock)->dep_map, _RET_IP_);	\
		____err = lock(_lock);				\
	}							\
	if (!____err)						\
		lock_acquired(&(_lock)->dep_map, _RET_IP_);	\
	____err;						\
})

#else /* CONFIG_LOCK_STAT */

#define lock_contended(lockdep_map, ip) do {} while (0)
#define lock_acquired(lockdep_map, ip) do {} while (0)

#define LOCK_CONTENDED(_lock, try, lock) \
	lock(_lock)

#define LOCK_CONTENDED_RETURN(_lock, try, lock) \
	lock(_lock)

#endif /* CONFIG_LOCK_STAT */













