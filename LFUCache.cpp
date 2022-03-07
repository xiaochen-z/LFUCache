#include <iostream>
#include <vector>
#include <unordered_map>
#include <list>
#include <concepts>
#include <cassert>

template<typename T>
concept DefaultContructible = std::is_default_constructible<T>::value;

template<typename K, typename V>
    requires DefaultContructible<V>
class LFUCache {
private:
    using LstIter = std::list<K>::iterator;

    struct KeyMeta {
        LstIter iter;
        int freq;
        V val;
    };

    size_t mCapacity;
    int mMinFreq; // minimum frequency of all keys
    std::unordered_map<int, std::list<K>> mKeysByFreq; // freq -> list of keys, the head of list is the key most recently used, the tail is least recently used
    std::unordered_map<K, KeyMeta> mKeyMetaByKey;      // key -> [iterator to key's pos in list, freq, value]

public:
    LFUCache(size_t capacity) : mCapacity(capacity), mMinFreq(0) {
        if (capacity <= 0) {
            throw std::invalid_argument ("Capacity cannot be less than or equal to zero.");
        }
    }
    ~LFUCache() = default;

    bool contains(K key) const {
        return mKeyMetaByKey.find(key) != mKeyMetaByKey.end();
    }

    bool empty() const {
        return mKeyMetaByKey.empty();
    }

    size_t size() const {
        return mKeyMetaByKey.size();
    }

    V get(K key) {
        if (contains(key)) {
            // cache hit
            touch(key);
            return mKeyMetaByKey[key].val;
        }

        // cache miss, we put a default constructed value in our cache
        V defaultVal = V();
        put(key, defaultVal);

        return defaultVal;
    }

    void put(K key, V val) {
        if (contains(key)) {
            // cache contains val, update existing entry
            touch(key);
            mKeyMetaByKey[key].val = val;
            return;
        }

        if (mKeyMetaByKey.size() == mCapacity) {
            evict();
        }

        mMinFreq = 1;
        mKeysByFreq[mMinFreq].push_front(key);
        KeyMeta curKeyMetaData {mKeysByFreq[mMinFreq].begin(), 1, val};
        mKeyMetaByKey[key] = curKeyMetaData;
    }

    void touch(K key) {
        auto keyMeta = mKeyMetaByKey[key];
        auto iter = keyMeta.iter;
        int oldFreq = keyMeta.freq;
        int newFreq = oldFreq + 1;

        mKeysByFreq[oldFreq].erase(iter); // delete entry in list of keys at old freq
        
        mKeyMetaByKey[key].freq = newFreq; // update freq
        mKeysByFreq[newFreq].push_front(key); // add entry at head of list of keys at new freq
        mKeyMetaByKey[key].iter = mKeysByFreq[newFreq].begin();

        if (mKeysByFreq[mMinFreq].empty()) {
            // as result of touch, if no element is of min freq, then min freq must be incremented
            mMinFreq += 1;
        }
    }

    void evict() {
        K key = mKeysByFreq[mMinFreq].back(); // key with least frequency and least recently used

        mKeyMetaByKey.erase(key);
        mKeysByFreq[mMinFreq].pop_back();
    }
};

// should put template in header file though..
template class LFUCache<int, int>;

int main() {
    /*
    1. instantiation with a non-default constructible type for value should
       give us compile error

    struct NonDefaultConstructible {
        NonDefaultConstructible() = delete;
    }

    LFUCache<int, NonDefaultConstructible> cache(1); <-- compile error

    2. construct with negative or zero capacity should throw invalid argument
       if using google test the test can be written as

        TEST(InvalidCapacity, NegativeCapacity) {
           EXPECT_THROW({
                try {
                    LFUCache<int, int> negCapCache(-1);
                }
                catch (const std::invalid_argument& err)
                {
                    EXPECT_STREQ("Capacity cannot be less than or equal to zero.", err.what());
                    throw;
                }
            }, std::invalid_argument);
        }

        TEST(InvalidCapacity, ZeroCapacity) {
           EXPECT_THROW({
                try {
                    LFUCache<int, int> zeroCapCache(0);
                }
                catch (const std::invalid_argument& err)
                {
                    EXPECT_STREQ("Capacity cannot be less than or equal to zero.", err.what());
                    throw;
                }
            }, std::invalid_argument);
        }

    3. as get method has a side effect of changing the frequency and freshness of key, it may be wise
       to have a side-effect-free get method for testing purpose

    4. during interview, I mainly got stuck on how the minimum frequency is changed when we evict the lowest
       frequently used element. It turns out when we call evict, it means there is a new element being inserted
       into the cache, so the new value for minimum frequency is 1 :) 

    */

    {
        // test put and get with existant key on one capacity cache
        LFUCache<int, int> oneCapCache(1);
        assert(oneCapCache.size() == 0);
        assert(oneCapCache.empty() == true);

        oneCapCache.put(1, 1);
        assert(oneCapCache.size() == 1);
        assert(oneCapCache.empty() == false);
        assert(oneCapCache.contains(1) == true);
        assert(oneCapCache.get(1) == 1);

        oneCapCache.put(2, 2);
        assert(oneCapCache.size() == 1);
        assert(oneCapCache.empty() == false);
        assert(oneCapCache.contains(1) == false);
        assert(oneCapCache.contains(2) == true);
        assert(oneCapCache.get(2) == 2);

        oneCapCache.put(2, 3);
        assert(oneCapCache.size() == 1);
        assert(oneCapCache.empty() == false);
        assert(oneCapCache.contains(1) == false);
        assert(oneCapCache.contains(2) == true);
        assert(oneCapCache.get(2) == 3);
    }

    {
        // test get with non-exist key on one capacity cache
        LFUCache<int, int> oneCapCache(1);
        assert(oneCapCache.size() == 0);
        assert(oneCapCache.empty() == true);

        oneCapCache.put(1, 1);
        assert(oneCapCache.size() == 1);
        assert(oneCapCache.empty() == false);
        assert(oneCapCache.contains(1) == true);
        assert(oneCapCache.get(1) == 1);

        assert(oneCapCache.get(2) == 0);  // get key not existed in cache, we should put and get a default constructed value
        assert(oneCapCache.size() == 1);
        assert(oneCapCache.empty() == false);
        assert(oneCapCache.contains(1) == false);
        assert(oneCapCache.contains(2) == true);
    }

    {
        // test put and get with existant key on many capacity cache
        LFUCache<int, int> manyCapCache(3);
        assert(manyCapCache.size() == 0);
        assert(manyCapCache.empty() == true);

        manyCapCache.put(1, 1);
        assert(manyCapCache.size() == 1);
        assert(manyCapCache.empty() == false);
        assert(manyCapCache.contains(1) == true);
        assert(manyCapCache.get(1) == 1);

        manyCapCache.put(2, 2);
        assert(manyCapCache.size() == 2);
        assert(manyCapCache.empty() == false);
        assert(manyCapCache.contains(1) == true);
        assert(manyCapCache.contains(2) == true);
        assert(manyCapCache.get(2) == 2);

        manyCapCache.put(2, 4);
        assert(manyCapCache.size() == 2);
        assert(manyCapCache.empty() == false);
        assert(manyCapCache.contains(1) == true);
        assert(manyCapCache.contains(2) == true);
        assert(manyCapCache.get(2) == 4);

        manyCapCache.put(3, 3);
        assert(manyCapCache.size() == 3);
        assert(manyCapCache.empty() == false);
        assert(manyCapCache.contains(1) == true);
        assert(manyCapCache.contains(2) == true);
        assert(manyCapCache.contains(3) == true);
        assert(manyCapCache.get(3) == 3);

        manyCapCache.put(4, 4);
        assert(manyCapCache.size() == 3);
        assert(manyCapCache.empty() == false);
        assert(manyCapCache.contains(1) == false); // both 1 and 3 are same frequency, 1 is least recently used
        assert(manyCapCache.contains(2) == true);
        assert(manyCapCache.contains(3) == true);
        assert(manyCapCache.contains(4) == true);
        assert(manyCapCache.get(4) == 4);
        
        manyCapCache.put(4, 5);
        assert(manyCapCache.size() == 3);
        assert(manyCapCache.empty() == false);
        assert(manyCapCache.contains(1) == false); 
        assert(manyCapCache.contains(2) == true);
        assert(manyCapCache.contains(3) == true);
        assert(manyCapCache.contains(4) == true);
        assert(manyCapCache.get(4) == 5);
        
        manyCapCache.put(5, 5);
        assert(manyCapCache.size() == 3);
        assert(manyCapCache.empty() == false);
        assert(manyCapCache.contains(1) == false); 
        assert(manyCapCache.contains(2) == true);
        assert(manyCapCache.contains(3) == false); // 3 is used 1 time, 2 is used 2 times, 3 should be evicted
        assert(manyCapCache.contains(4) == true);
        assert(manyCapCache.contains(5) == true);
        assert(manyCapCache.get(5) == 5);
    }

    {
        // test get with non-exist key on many capacity cache
        LFUCache<int, int> manyCapCache(3);
        assert(manyCapCache.size() == 0);
        assert(manyCapCache.empty() == true);

        manyCapCache.put(1, 1);
        assert(manyCapCache.size() == 1);
        assert(manyCapCache.empty() == false);
        assert(manyCapCache.contains(1) == true);
        assert(manyCapCache.get(1) == 1);

        manyCapCache.put(2, 2);
        assert(manyCapCache.size() == 2);
        assert(manyCapCache.empty() == false);
        assert(manyCapCache.contains(1) == true);
        assert(manyCapCache.contains(2) == true);
        assert(manyCapCache.get(2) == 2);

        manyCapCache.put(3, 3);
        assert(manyCapCache.size() == 3);
        assert(manyCapCache.empty() == false);
        assert(manyCapCache.contains(1) == true);
        assert(manyCapCache.contains(2) == true);
        assert(manyCapCache.contains(3) == true);
        assert(manyCapCache.get(3) == 3);

        assert(manyCapCache.get(4) == 0); // get key not existed in cache, we should put and get a default constructed value
        assert(manyCapCache.size() == 3);
        assert(manyCapCache.empty() == false);
        assert(manyCapCache.contains(1) == false);
        assert(manyCapCache.contains(2) == true);
        assert(manyCapCache.contains(3) == true);    
        assert(manyCapCache.contains(4) == true);
    }

}
