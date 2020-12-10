
struct event_list
{
    struct event *tqh_first;
    struct event **tqh_last;
};
 
struct evmap_io
{
    struct event_list events;
    ev_uint16_t nread;
    ev_uint16_t nwrite;
};
 
 
struct event_map_entry
{
    struct
    {
        struct event_map_entry *hte_next;
#ifdef HT_CACHE_HASH_VALUES
        unsigned hte_hash;
#endif
    }map_node;
 
    evutil_socket_t fd;
    union
    {
        struct evmap_io evmap_io;
    }ent;
};
 
struct event_io_map
{
    //哈希表
    struct event_map_entry **hth_table;
    //哈希表的长度
    unsigned hth_table_length;
    //哈希的元素个数
    unsigned hth_n_entries;
    //resize 之前可以存多少个元素
    //在event_io_map_HT_GROW函数中可以看到其值为hth_table_length的
    //一半。但hth_n_entries>=hth_load_limit时，就会发生增长哈希表的长度
    unsigned hth_load_limit;
    //后面素数表中的下标值。主要是指明用到了哪个素数
    int hth_prime_idx;
};
 
 
 
int event_io_map_HT_GROW(struct event_io_map *ht, unsigned min_capacity);
void event_io_map_HT_CLEAR(struct event_io_map *ht);
 
int _event_io_map_HT_REP_IS_BAD(const struct event_io_map *ht);
 
//初始化event_io_map
static inline void event_io_map_HT_INIT(struct event_io_map *head)
{
    head->hth_table_length = 0;
    head->hth_table = NULL;
    head->hth_n_entries = 0;
    head->hth_load_limit = 0;
    head->hth_prime_idx = -1;
}
 
 
//在event_io_map这个哈希表中，找个表项elm
//在下面还有一个相似的函数，函数名少了最后的_P。那个函数的返回值
//是event_map_entry *。从查找来说，后面那个函数更适合。之所以
//有这个函数，是因为哈希表还有replace、remove这些操作。对于
//A->B->C这样的链表。此时，要replace或者remove节点B。
//如果只有后面那个查找函数，那么只能查找并返回一个指向B的指针。
//此时将无法修改A的next指针了。所以本函数有存在的必要。
//在本文件中，很多函数都使用了event_map_entry **。
//因为event_map_entry **类型变量，既可以修改本元素的hte_next变量
//也能指向下一个元素。
 
//该函数返回的是查找节点的前驱节点的hte_next成员变量的地址。
//所以返回值肯定不会为NULL,但是对返回值取*就可能为NULL
static inline struct event_map_entry **
_event_io_map_HT_FIND_P(struct event_io_map *head,
                        struct event_map_entry *elm)
{
    struct event_map_entry **p;
    if (!head->hth_table)
        return NULL;
 
#ifdef HT_CACHE_HASH_VALUES
    p = &((head)->hth_table[((elm)->map_node.hte_hash)
            % head->hth_table_length]);
#else
    p = &((head)->hth_table[(hashsocket(*elm))%head->hth_table_length]);
#endif
 
    //这里的哈希表是用链地址法解决哈希冲突的。
    //上面的 % 只是找到了冲突链的头。现在是在冲突链中查找。
    while (*p)
    {
        //判断是否相等。在实现上，只是简单地根据fd来判断是否相等
        if (eqsocket(*p, elm))
            return p;
 
        //p存放的是hte_next成员变量的地址
        p = &(*p)->map_node.hte_next;
    }
 
    return p;
}
 
/* Return a pointer to the element in the table 'head' matching 'elm',
 * or NULL if no such element exists */
//在event_io_map这个哈希表中，找个表项elm
static inline struct event_map_entry *
event_io_map_HT_FIND(const struct event_io_map *head,
                     struct event_map_entry *elm)
{
    struct event_map_entry **p;
    struct event_io_map *h = (struct event_io_map *) head;
 
#ifdef HT_CACHE_HASH_VALUES
    do
    {   //计算哈希值
        (elm)->map_node.hte_hash = hashsocket(elm);
    } while(0);
#endif
 
    p = _event_io_map_HT_FIND_P(h, elm);
    return p ? *p : NULL;
}
 
 
/* Insert the element 'elm' into the table 'head'.  Do not call this
 * function if the table might already contain a matching element. */
static inline void
event_io_map_HT_INSERT(struct event_io_map *head,
                       struct event_map_entry *elm)
{
    struct event_map_entry **p;
    if (!head->hth_table || head->hth_n_entries >= head->hth_load_limit)
        event_io_map_HT_GROW(head, head->hth_n_entries+1);
 
    ++head->hth_n_entries;
 
#ifdef HT_CACHE_HASH_VALUES
    do
    {   //计算哈希值.此哈希不同于用%计算的简单哈希。
        //存放到hte_hash变量中，作为cache
        (elm)->map_node.hte_hash = hashsocket(elm);
    } while (0);
 
    p = &((head)->hth_table[((elm)->map_node.hte_hash)
            % head->hth_table_length]);
#else
    p = &((head)->hth_table[(hashsocket(*elm))%head->hth_table_length]);
#endif
 
 
    //使用头插法，即后面才插入的链表，反而会在链表头。
    elm->map_node.hte_next = *p;
    *p = elm;
}
 
 
/* Insert the element 'elm' into the table 'head'. If there already
 * a matching element in the table, replace that element and return
 * it. */
static inline struct event_map_entry *
event_io_map_HT_REPLACE(struct event_io_map *head,
                        struct event_map_entry *elm)
{
    struct event_map_entry **p, *r;
 
    if (!head->hth_table || head->hth_n_entries >= head->hth_load_limit)
        event_io_map_HT_GROW(head, head->hth_n_entries+1);
 
#ifdef HT_CACHE_HASH_VALUES
    do
    {
        (elm)->map_node.hte_hash = hashsocket(elm);
    } while(0);
#endif
 
    p = _event_io_map_HT_FIND_P(head, elm);
 
    //由前面的英文注释可知，这个函数是替换插入一起进行的。如果哈希表
    //中有和elm相同的元素(指的是event_map_entry的fd成员变量值相等)
    //那么就发生替代(其他成员变量值不同，所以不是完全相同，有替换意义)
    //如果哈希表中没有和elm相同的元素，那么就进行插入操作
 
    //指针p存放的是hte_next成员变量的地址
    //这里的p存放的是被替换元素的前驱元素的hte_next变量地址
    r = *p; //r指向了要替换的元素。有可能为NULL
    *p = elm; //hte_next变量被赋予新值elm
 
    //找到了要被替换的元素r(不为NULL)
    //而且要插入的元素地址不等于要被替换的元素地址
    if (r && (r!=elm))
    {
        elm->map_node.hte_next = r->map_node.hte_next;
 
        r->map_node.hte_next = NULL;
        return r; //返回被替换掉的元素
    }
    else //进行插入操作
    {
        //这里貌似有一个bug。如果前一个判断中，r 不为 NULL，而且r == elm
        //对于同一个元素，多次调用本函数。就会出现这样情况。
        //此时，将会来到这个else里面
        //实际上没有增加元素，但元素的个数却被++了。因为r 的地址等于 elm
        //所以 r = *p; *p = elm; 等于什么也没有做。（r == elm)
		//当然，因为这些函数都是Libevent内部使用的。如果它保证不会同于同
		//一个元素调用本函数，那么就不会出现bug
        ++head->hth_n_entries;
        return NULL; //插入操作返回NULL，表示没有替换到元素
    }
}
 
 
/* Remove any element matching 'elm' from the table 'head'.  If such
 * an element is found, return it; otherwise return NULL. */
static inline struct event_map_entry *
event_io_map_HT_REMOVE(struct event_io_map *head,
                       struct event_map_entry *elm)
{
    struct event_map_entry **p, *r;
 
#ifdef HT_CACHE_HASH_VALUES
    do
    {
        (elm)->map_node.hte_hash = hashsocket(elm);
    } while (0);
#endif
 
    p = _event_io_map_HT_FIND_P(head,elm);
 
    if (!p || !*p)//没有找到
        return NULL;
 
    //指针p存放的是hte_next成员变量的地址
    //这里的p存放的是被替换元素的前驱元素的hte_next变量地址
    r = *p; //r现在指向要被删除的元素
    *p = r->map_node.hte_next;
    r->map_node.hte_next = NULL;
 
    --head->hth_n_entries;
 
    return r;
}
 
 
/* Invoke the function 'fn' on every element of the table 'head',
 using 'data' as its second argument.  If the function returns
 nonzero, remove the most recently examined element before invoking
 the function again. */
static inline void
event_io_map_HT_FOREACH_FN(struct event_io_map *head,
                           int (*fn)(struct event_map_entry *, void *),
                           void *data)
{
    unsigned idx;
    struct event_map_entry **p, **nextp, *next;
 
    if (!head->hth_table)
        return;
 
    for (idx=0; idx < head->hth_table_length; ++idx)
    {
        p = &head->hth_table[idx];
 
        while (*p)
        {
            //像A->B->C链表。p存放了A元素中hte_next变量的地址
            //*p则指向B元素。nextp存放B的hte_next变量的地址
            //next指向C元素。
            nextp = &(*p)->map_node.hte_next;
            next = *nextp;
 
            //对B元素进行检查
            if (fn(*p, data))
            {
                --head->hth_n_entries;
                //p存放了A元素中hte_next变量的地址
                //所以*p = next使得A元素的hte_next变量值被赋值为
                //next。此时链表变成A->C。即使抛弃了B元素。不知道
                //调用方是否能释放B元素的内存。
                *p = next;
            }
            else
            {
                p = nextp;
            }
        }
    }
}
 
 
/* Return a pointer to the first element in the table 'head', under
 * an arbitrary order.  This order is stable under remove operations,
 * but not under others. If the table is empty, return NULL. */
//获取第一条冲突链的第一个元素
static inline struct event_map_entry **
event_io_map_HT_START(struct event_io_map *head)
{
    unsigned b = 0;
 
    while (b < head->hth_table_length)
    {
        //返回哈希表中，第一个不为NULL的节点
        //即有event_map_entry元素的节点。
        //找到链。因为是哈希。所以不一定哈希表中的每一个节点都存有元素
        if (head->hth_table[b])
            return &head->hth_table[b];
 
        ++b;
    }
 
    return NULL;
}
 
 
 
/* Return the next element in 'head' after 'elm', under the arbitrary
 * order used by HT_START.  If there are no more elements, return
 * NULL.  If 'elm' is to be removed from the table, you must call
 * this function for the next value before you remove it.
 */
static inline struct event_map_entry **
event_io_map_HT_NEXT(struct event_io_map *head,
                     struct event_map_entry **elm)
{
    //本哈希解决冲突的方式是链地址
    //如果参数elm所在的链地址中，elm还有下一个节点，就直接返回下一个节点
    if ((*elm)->map_node.hte_next)
    {
        return &(*elm)->map_node.hte_next;
    }
    else //否则取哈希表中的下一条链中第一个元素
    {
#ifdef HT_CACHE_HASH_VALUES
        unsigned b = (((*elm)->map_node.hte_hash)
                      % head->hth_table_length) + 1;
#else
        unsigned b = ( (hashsocket(*elm)) % head->hth_table_length) + 1;
#endif
 
        while (b < head->hth_table_length)
        {
            //找到链。因为是哈希。所以不一定哈希表中的每一个节点都存有元素
            if (head->hth_table[b])
                return &head->hth_table[b];
            ++b;
        }
 
        return NULL;
    }
}
 
 
 
//功能同上一个函数。只是参数不同，另外本函数还会使得--hth_n_entries
//该函数主要是返回elm的下一个元素。并且哈希表的总元素个数减一。
//主调函数会负责释放*elm指向的元素。无需在这里动手
//在evmap_io_clear函数中，会调用本函数。
static inline struct event_map_entry **
event_io_map_HT_NEXT_RMV(struct event_io_map *head,
                         struct event_map_entry **elm)
{
#ifdef HT_CACHE_HASH_VALUES
    unsigned h = ((*elm)->map_node.hte_hash);
#else
    unsigned h = (hashsocket(*elm));
#endif
 
    //elm变量变成存放下一个元素的hte_next的地址
    *elm = (*elm)->map_node.hte_next;
 
    --head->hth_n_entries;
 
    if (*elm)
    {
        return elm;
    }
    else
    {
        unsigned b = (h % head->hth_table_length)+1;
 
        while (b < head->hth_table_length)
        {
            if (head->hth_table[b])
                return &head->hth_table[b];
 
            ++b;
        }
 
        return NULL;
    }
}
 
 
 
//素数表。之所以去素数，是因为在取模的时候，素数比合数更有优势。
//听说是更散，更均匀
static unsigned event_io_map_PRIMES[] =
{
    //素数表的元素具有差不多2倍的关系。
    //这使得扩容操作不会经常发生。每次扩容都预留比较大的空间
    53, 97, 193, 389, 769, 1543, 3079,
    6151, 12289, 24593, 49157, 98317,
    196613, 393241, 786433, 1572869, 3145739,
    6291469, 12582917, 25165843, 50331653, 100663319,
    201326611, 402653189, 805306457, 1610612741
};
 
 
//素数表中，元素的个数。
static unsigned event_io_map_N_PRIMES =
        (unsigned)(sizeof(event_io_map_PRIMES)
                   /sizeof(event_io_map_PRIMES[0]));
 
 
/* Expand the internal table of 'head' until it is large enough to
 * hold 'size' elements.  Return 0 on success, -1 on allocation
 * failure. */
int event_io_map_HT_GROW(struct event_io_map *head, unsigned size)
{
    unsigned new_len, new_load_limit;
    int prime_idx;
 
    struct event_map_entry **new_table;
    //已经用到了素数表中最后一个素数，不能再扩容了。
    if (head->hth_prime_idx == (int)event_io_map_N_PRIMES - 1)
        return 0;
 
    //哈希表中还够容量，无需扩容
    if (head->hth_load_limit > size)
        return 0;
 
    prime_idx = head->hth_prime_idx;
 
    do {
        new_len = event_io_map_PRIMES[++prime_idx];
 
        //从素数表中的数值可以看到，后一个差不多是前一个的2倍。
        //从0.5和后的new_load_limit <= size，可以得知此次扩容
        //至少得是所需大小(size)的2倍以上。免得经常要进行扩容
        new_load_limit = (unsigned)(0.5*new_len);
    } while (new_load_limit <= size
             && prime_idx < (int)event_io_map_N_PRIMES);
 
    if ((new_table = mm_malloc(new_len*sizeof(struct event_map_entry*))))
    {
        unsigned b;
        memset(new_table, 0, new_len*sizeof(struct event_map_entry*));
 
        for (b = 0; b < head->hth_table_length; ++b)
        {
            struct event_map_entry *elm, *next;
            unsigned b2;
 
            elm = head->hth_table[b];
            while (elm) //该节点有冲突链。遍历冲突链中所有的元素
            {
                next = elm->map_node.hte_next;
 
                //冲突链中的元素，相对于前一个素数同余(即模素数后，结果相当)
                //但对于现在的新素数就不一定同余了，即它们不一定还会冲突
                //所以要对冲突链中的所有元素都再次哈希，并放到它们应该在的地方
                //b2存放了再次哈希后，元素应该存放的节点下标。
#ifdef HT_CACHE_HASH_VALUES
                b2 = (elm)->map_node.hte_hash % new_len;
#else
                b2 = (hashsocket(*elm)) % new_len;
#endif
                //用头插法插入数据
                elm->map_node.hte_next = new_table[b2];
                new_table[b2] = elm;
 
                elm = next;
            }
        }
 
        if (head->hth_table)
            mm_free(head->hth_table);
 
        head->hth_table = new_table;
    }
    else
    {
        unsigned b, b2;
 
        //刚才mm_malloc失败，可能是内存不够。现在用更省内存的
        //mm_realloc方式。当然其代价就是更耗时(下面的代码可以看到)。
        //前面的mm_malloc会同时有hth_table和new_table两个数组。
        //而mm_realloc则只有一个数组，所以省内存，省了一个hth_table数组
        //的内存。此时，new_table数组的前head->hth_table_length个
        //元素存放了原来的冲突链的头部。也正是这个原因导致后面代码更耗时。
        //其实，只有在很特殊的情况下，这个函数才会比mm_malloc省内存.
        //就是堆内存head->hth_table区域的后面刚好有一段可以用的内存。
        //具体的，可以搜一下realloc这个函数。
        new_table = mm_realloc(head->hth_table,
                               new_len*sizeof(struct event_map_entry*));
 
        if (!new_table)
            return -1;
 
        memset(new_table + head->hth_table_length, 0,
               (new_len - head->hth_table_length)*sizeof(struct event_map_entry*)
               );
 
        for (b=0; b < head->hth_table_length; ++b)
        {
            struct event_map_entry *e, **pE;
 
            for (pE = &new_table[b], e = *pE; e != NULL; e = *pE)
            {
 
#ifdef HT_CACHE_HASH_VALUES
                b2 = (e)->map_node.hte_hash % new_len;
#else
                b2 = (hashsocket(*elm)) % new_len;
#endif
                //对于冲突链A->B->C.
                //pE是二级指针，存放的是A元素的hte_next指针的地址值
                //e指向B元素。
 
                //对新的素数进行哈希，刚好又在原来的位置
                if (b2 == b)
                {
                    //此时，无需修改。接着处理冲突链中的下一个元素即可
                    //pE向前移动，存放B元素的hte_next指针的地址值
                    pE = &e->map_node.hte_next;
                }
                else//这个元素会去到其他位置上。
                {
                    //此时冲突链修改成A->C。
                    //所以pE无需修改，还是存放A元素的hte_next指针的地址值
                    //但A元素的hte_next指针要指向C元素。用*pE去修改即可
                    *pE = e->map_node.hte_next;
 
                    //将这个元素放到正确的位置上。
                    e->map_node.hte_next = new_table[b2];
                    new_table[b2] = e;
                }
 
                //这种再次哈希的方式，很有可能会对某些元素操作两次。
                //当某个元素第一次在else中处理，那么它就会被哈希到正确的节点
                //的冲突链上。随着外循环的进行，处理到正确的节点时。在遍历该节点
                //的冲突链时，又会再次处理该元素。此时，就会在if中处理。而不会
                //进入到else中。
            }
        }
 
        head->hth_table = new_table;
    }
 
 
    //一般是当hth_n_entries >= hth_load_limit时，就会调用
    //本函数。hth_n_entries表示的是哈希表中节点的个数。而hth_load_limit
    //是hth_table_length的一半。hth_table_length则是哈希表中
    //哈希函数被模的数字。所以，当哈希表中的节点个数到达哈希表长度的一半时
    //就会发生增长，本函数被调用。这样的结果是：平均来说，哈希表应该比较少发生
    //冲突。即使有，冲突链也不会太长。这样就能有比较快的查找速度。
    head->hth_table_length = new_len;
    head->hth_prime_idx = prime_idx;
    head->hth_load_limit = new_load_limit;
 
    return 0;
}
 
 
/* Free all storage held by 'head'.  Does not free 'head' itself,
 * or individual elements. 并不需要释放独立的元素*/
//在evmap_io_clear函数会调用该函数。其是在删除所有哈希表中的元素后
//才调用该函数的。
void event_io_map_HT_CLEAR(struct event_io_map *head)
{
    if (head->hth_table)
        mm_free(head->hth_table);
 
    head->hth_table_length = 0;
 
    event_io_map_HT_INIT(head);
}
 
 
/* Debugging helper: return false iff the representation of 'head' is
 * internally consistent. */
int _event_io_map_HT_REP_IS_BAD(const struct event_io_map *head)
{
    unsigned n, i;
    struct event_map_entry *elm;
 
    if (!head->hth_table_length)
    {
        //刚被初始化，还没申请任何空间
        if (!head->hth_table && !head->hth_n_entries
                && !head->hth_load_limit && head->hth_prime_idx == -1
                )
            return 0;
        else
            return 1;
    }
 
    if (!head->hth_table || head->hth_prime_idx < 0
            || !head->hth_load_limit
            )
        return 2;
 
    if (head->hth_n_entries > head->hth_load_limit)
        return 3;
 
    if (head->hth_table_length != event_io_map_PRIMES[head->hth_prime_idx])
        return 4;
 
    if (head->hth_load_limit != (unsigned)(0.5*head->hth_table_length))
        return 5;
 
    for (n = i = 0; i < head->hth_table_length; ++i)
    {
        for (elm = head->hth_table[i]; elm; elm = elm->map_node.hte_next)
        {
 
#ifdef HT_CACHE_HASH_VALUES
 
            if (elm->map_node.hte_hash != hashsocket(elm))
                return 1000 + i;
 
            if( (elm->map_node.hte_hash % head->hth_table_length) != i)
                return 10000 + i;
 
#else
            if ( (hashsocket(*elm)) != hashsocket(elm))
                return 1000 + i;
 
            if( ( (hashsocket(*elm)) % head->hth_table_length) != i)
                return 10000 + i;
#endif
            ++n;
        }
    }
 
    if (n != head->hth_n_entries)
        return 6;
 
    return 0;
}