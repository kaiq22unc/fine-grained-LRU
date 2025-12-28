# lab-4-threaded-lru-cache-kaiq_lab4
locking protocol for fine grained locking:
When traversing the list, hold a global lock until traverse past the list head, since modifying list_head, a global var, requires global lock. After node last is past list_head, we can release the global lock and hold
only 2 locks while traversing the list: one lock for node last and one lock for node cursor. As we move the cursor forward, just release the node last lock before we moving the cursor, then the current
cursor lock would become last and we can acquire the new cursor lock as the cursor lock.
Note that when modifying count and signaling cv would also require global lock. So, we acquire global lock if we do not have it already before that.
