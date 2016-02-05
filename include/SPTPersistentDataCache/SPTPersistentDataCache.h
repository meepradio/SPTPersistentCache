/*
 * Copyright (c) 2016 Spotify AB.
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
#import <Foundation/Foundation.h>

#import "SPTPersistentCacheTypes.h"
#import "SPTPersistentDataCacheOptions.h"

/**
 * @brief SPTPersistentDataCache
 *
 * @discussion Class defines persistent cache that manage files on disk. This class is threadsafe.
 * Except methods for scheduling/unscheduling GC which must be called on main thread.
 * It is obligatory that one instanse of that class manage one path branch on disk. In other case behavior is undefined.
 * Cache uses own queue for all operations.
 * Cache GC procedure evicts all not locked files for which current_gc_time - access_time > defaultExpirationPeriodSec.
 * Cache GC procedure evicts all not locked files for which current_gc_time - creation_time > fileTTL.
 * Files that are locked not evicted by GC procedure and returned by the cache even if they already expired. 
 * Once unlocked, expired files would be collected by following GC
 * Req.#1.3 record opened as stream couldn't be altered by usual cache methods and doesn't take part in locked size calculation.
 */
@interface SPTPersistentDataCache : NSObject

- (instancetype)initWithOptions:(SPTPersistentDataCacheOptions *)options;

/**
 * @discussion Load data from cache for specified key. 
 *             Req.#1.2. Expired records treated as not found on load. (And open stream)
 *
 * @param key Key used to access the data. It MUST MUST MUST be unique for different data. 
 *            It could be used as a part of file name. It up to a cache user to define algorithm to form a key.
 * @param callback callback to call once data is loaded. It mustn't be nil.
 * @param queue Queue on which to run the callback. Mustn't be nil.
 */
- (void)loadDataForKey:(NSString *)key
          withCallback:(SPTDataCacheResponseCallback)callback
               onQueue:(dispatch_queue_t)queue;


/**
 * @discussion Load data for key which has specified prefix. chooseKeyCallback is called with array of matching keys.
 *             Req.#1.1a. To load the data user needs to pick one key and return it.
 *             Req.#1.1b. If non of those are match then return nil and cache will return not found error.
 *             chooseKeyCallback is called on any thread and caller should not do any heavy job in it.
 *             Req.#1.2. Expired records treated as not found on load. (And open stream)
 *
 * @param prefix Prefix which key should have to be candidate for loading.
 * @param chooseKeyCallback callback to call to define which key to use to load the data. 
 * @param callback callback to call once data is loaded. It mustn't be nil.
 * @param queue Queue on which to run the callback. Mustn't be nil.
 */
- (void)loadDataForKeysWithPrefix:(NSString *)prefix
                chooseKeyCallback:(SPTDataCacheChooseKeyCallback)chooseKeyCallback
                     withCallback:(SPTDataCacheResponseCallback)callback
                          onQueue:(dispatch_queue_t)queue;

/**
 * @discussion Req.#1.0. If data already exist for that key it will be overwritten otherwise created.
 * Its access time will be updated. RefCount depends on locked parameter.
 * Data is expired when current_gc_time - access_time > defaultExpirationPeriodSec.
 *
 * @param data Data to store. Mustn't be nil
 * @param key Key to associate the data with.
 * @param locked If YES then data refCount is set to 1. If NO then set to 0.
 * @param callback Callback to call once data is loaded. Could be nil.
 * @param queue Queue on which to run the callback. Couldn't be nil if callback is specified.
 */
- (void)storeData:(NSData *)data
           forKey:(NSString *)key
           locked:(BOOL)locked
     withCallback:(SPTDataCacheResponseCallback)callback
          onQueue:(dispatch_queue_t)queue;


/**
 * @discussion Req.#1.0. If data already exist for that key it will be overwritten otherwise created.
 * Its access time will be apdated. Its TTL will be updated if applicable.
 * RefCount depends on locked parameter.
 * Data is expired when current_gc_time - access_time > TTL.
 *
 * @param data Data to store. Mustn't be nil.
 * @param key Key to associate the data with.
 * @param ttl TTL value for a file. 0 is equivalent to storeData:forKey: behavior.
 * @param locked If YES then data refCount is set to 1. If NO then set to 0.
 * @param callback Callback to call once data is loaded. Could be nil.
 * @param queue Queue on which to run the callback. Couldn't be nil if callback is specified.
 */
- (void)storeData:(NSData *)data
           forKey:(NSString *)key
              ttl:(NSUInteger)ttl
           locked:(BOOL)locked
     withCallback:(SPTDataCacheResponseCallback)callback
          onQueue:(dispatch_queue_t)queue;

/**
 * @discussion Update last access time in header of the record. Only applies for default expiration policy (ttl == 0).
 *             Locked files could be touched even if they are expired.
 *             Success callback is given if file was found and no errors occured even though nothing was changed due to ttl == 0.
 *             Req.#1.2. Expired records treated as not found on touch.
 *
 * @param key Key which record header to update. Mustn't be nil.
 * @param callback. May be nil if not interested in result.
 * @param queue Queue on which to run the callback. If callback is nil this is ignored otherwise mustn't be nil.
 */
- (void)touchDataForKey:(NSString *)key
               callback:(SPTDataCacheResponseCallback)callback
                onQueue:(dispatch_queue_t)queue;

/**
 * @brief Removes data for keys unconditionally even if expired.
 */
- (void)removeDataForKeys:(NSArray *)keys;

/**
 * @discussion Increment ref count for given keys. Give callback with result for each key in input array.
 *             Req.#1.2. Expired records treated as not found on lock.
 *
 * @param keys Non nil non empty array of keys.
 * @param callback. May be nil if not interested in result.
 * @param queue Queue on which to run the callback. If callback is nil this is ignored otherwise mustn't be nil.
 */
- (void)lockDataForKeys:(NSArray *)keys
               callback:(SPTDataCacheResponseCallback)callback
                onQueue:(dispatch_queue_t)queue;

/**
 * @discussion Decrement ref count for given keys. Give callback with result for each key in input array.
 *             If decrements exceeds increments assertion is given.
 *
 * @param keys Non nil non empty array of keys.
 * @param callback. May be nil if not interested in result.
 * @param queue Queue on which to run the callback. If callback is nil this is ignored otherwise mustn't be nil.
 */
- (void)unlockDataForKeys:(NSArray *)keys
                 callback:(SPTDataCacheResponseCallback)callback
                  onQueue:(dispatch_queue_t)queue;

/**
 * Schedule ragbage collection. If already scheduled then this method does nothing.
 * WARNING: This method has to be called on main thread.
 */
- (void)scheduleGarbageCollector;

/**
 * Stop ragbage collection. If already stopped this method does nothing.
 * WARNING: This method has to be called on main thread.
 */
- (void)unscheduleGarbageCollector;

/**
 * Delete all files files in managed folder unconditionaly.
 */
- (void)prune;

/**
 * Wipe only files that locked regardless of refCount value.
 */
- (void)wipeLockedFiles;

/**
 * Wipe only files that are not locked regardles of their expiration time.
 */
- (void)wipeNonLockedFiles;

/**
 * Returns size occupied by cache.
 * WARNING: This method does synchronous calculations.
 * WARNING: Files opened as streams are accounted in this calculations.
 */
- (NSUInteger)totalUsedSizeInBytes;

/**
 * Returns size occupied by locked items.
 * WARNING: This method does synchronous calculations.
 * WARNING: Files opened as streams are NOT accounted in this calculations.
 */
- (NSUInteger)lockedItemsSizeInBytes;

@end