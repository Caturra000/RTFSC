

// 观察register过程中sticky的特殊处理


// R上的签名已更改WithFeature
public Intent registerReceiverWithFeature(IApplicationThread caller, String callerPackage,
        String callerFeatureId, IIntentReceiver receiver, IntentFilter filter,
        String permission, int userId, int flags) {
    enforceNotIsolatedCaller("registerReceiver");

    // 所有sticky
    ArrayList<Intent> stickyIntents = null;



// SKIP START

    ProcessRecord callerApp = null;
    final boolean visibleToInstantApps
        = (flags & Context.RECEIVER_VISIBLE_TO_INSTANT_APPS) != 0;
    int callingUid;
    int callingPid;
    boolean instantApp;
    synchronized(this) {
        if (caller != null) {
            callerApp = getRecordForAppLocked(caller);
            if (callerApp == null) {
                throw new SecurityException(
                        "Unable to find app for caller " + caller
                        + " (pid=" + Binder.getCallingPid()
                        + ") when registering receiver " + receiver);
            }
            if (callerApp.info.uid != SYSTEM_UID &&
                    !callerApp.pkgList.containsKey(callerPackage) &&
                    !"android".equals(callerPackage)) {
                throw new SecurityException("Given caller package " + callerPackage
                        + " is not running in process " + callerApp);
            }
            callingUid = callerApp.info.uid;
            callingPid = callerApp.pid;
        } else {
            callerPackage = null;
            callingUid = Binder.getCallingUid();
            callingPid = Binder.getCallingPid();
        }

        instantApp = isInstantApp(callerApp, callerPackage, callingUid);
        userId = mUserController.handleIncomingUser(callingPid, callingUid, userId, true,
                ALLOW_FULL_ONLY, "registerReceiver", callerPackage);



// SKIP END




        Iterator<String> actions = filter.actionsIterator();
        if (actions == null) {
            ArrayList<String> noAction = new ArrayList<String>(1);
            noAction.add(null);
            actions = noAction.iterator();
        }

        // 从filter中获取actionIter actions, 并通过mStickyBroadcasts(key为action)获取sticky       // TODO filter
        // Collect stickies of users
        int[] userIds = { UserHandle.USER_ALL, UserHandle.getUserId(callingUid) };
        while (actions.hasNext()) {
            String action = actions.next();
            for (int id : userIds) {
                ArrayMap<String, ArrayList<Intent>> stickies = mStickyBroadcasts.get(id);     // mStickyBroadcasts的add行为位于broadcastIntentLocked
                if (stickies != null) {
                    ArrayList<Intent> intents = stickies.get(action);
                    if (intents != null) {
                        if (stickyIntents == null) { // 所以为什么不定义stickyIntents时就new，这样判空如果actions.hasNext没有执行应该还是会有问题
                            stickyIntents = new ArrayList<Intent>();
                        }
                        stickyIntents.addAll(intents);
                    }
                }
            }
        }
    }

    // 观察后面与stickyIntents的关系
    ArrayList<Intent> allSticky = null;

    if (stickyIntents != null) {
        final ContentResolver resolver = mContext.getContentResolver();                          // TODO resolver应该是ContentProvider那边的，为什么需要这样的行为？
        // Look for any matching sticky broadcasts...
        // 将stickyIntent中的intent进一步筛选写入allSticky
        for (int i = 0, N = stickyIntents.size(); i < N; i++) {
            Intent intent = stickyIntents.get(i);
            // Don't provided intents that aren't available to instant apps.
            if (instantApp &&
                    (intent.getFlags() & Intent.FLAG_RECEIVER_VISIBLE_TO_INSTANT_APPS) == 0) {
                continue;
            }
            // If intent has scheme "content", it will need to acccess
            // provider that needs to lock mProviderMap in ActivityThread
            // and also it may need to wait application response, so we
            // cannot lock ActivityManagerService here.
                                                                                                    // TODO resolver
            if (filter.match(resolver, intent, true, TAG) >= 0) {
                if (allSticky == null) {
                    allSticky = new ArrayList<Intent>();
                }
                allSticky.add(intent);
            }
        }
    }



    // 这里开始处理receiver相关

    // The first sticky in the list is returned directly back to the client.                              // TODO 为什么是first 什么时候index=0的sticky会remove?
    Intent sticky = allSticky != null ? allSticky.get(0) : null;
    if (DEBUG_BROADCAST) Slog.v(TAG_BROADCAST, "Register receiver " + filter + ": " + sticky);
    if (receiver == null) {
        return sticky;
    }

    synchronized (this) {
        if (callerApp != null && (callerApp.thread == null
                || callerApp.thread.asBinder() != caller.asBinder())) {
            // Original caller already died
            return null;
        }
        // 注意这里不是filter了
        ReceiverList rl = mRegisteredReceivers.get(receiver.asBinder());
        if (rl == null) {
            rl = new ReceiverList(this, callerApp, callingPid, callingUid,
                    userId, receiver);
            if (rl.app != null) {
                final int totalReceiversForApp = rl.app.receivers.size();
                if (totalReceiversForApp >= MAX_RECEIVERS_ALLOWED_PER_APP) {
                    throw new IllegalStateException("Too many receivers, total of "
                            + totalReceiversForApp + ", registered for pid: "
                            + rl.pid + ", callerPackage: " + callerPackage);
                }
                rl.app.receivers.add(rl);
            } else {
                try {
                    receiver.asBinder().linkToDeath(rl, 0);
                } catch (RemoteException e) {
                    return sticky;
                }
                rl.linkedToDeath = true;
            }
            mRegisteredReceivers.put(receiver.asBinder(), rl);
        } else if (rl.uid != callingUid) {
            throw new IllegalArgumentException(
                    "Receiver requested to register for uid " + callingUid
                    + " was previously registered for uid " + rl.uid
                    + " callerPackage is " + callerPackage);
        } else if (rl.pid != callingPid) {
            throw new IllegalArgumentException(
                    "Receiver requested to register for pid " + callingPid
                    + " was previously registered for pid " + rl.pid
                    + " callerPackage is " + callerPackage);
        } else if (rl.userId != userId) {
            throw new IllegalArgumentException(
                    "Receiver requested to register for user " + userId
                    + " was previously registered for user " + rl.userId
                    + " callerPackage is " + callerPackage);
        }
        BroadcastFilter bf = new BroadcastFilter(filter, rl, callerPackage, callerFeatureId,
                permission, callingUid, userId, instantApp, visibleToInstantApps);
        if (rl.containsFilter(filter)) {
            Slog.w(TAG, "Receiver with filter " + filter
                    + " already registered for pid " + rl.pid
                    + ", callerPackage is " + callerPackage);
        } else {
            rl.add(bf);
            if (!bf.debugCheck()) {
                Slog.w(TAG, "==> For Dynamic broadcast");
            }
            mReceiverResolver.addFilter(bf);
        }

        // Enqueue broadcasts for all existing stickies that match
        // this filter.
        if (allSticky != null) {
            ArrayList receivers = new ArrayList();
            receivers.add(bf);

            final int stickyCount = allSticky.size();
            for (int i = 0; i < stickyCount; i++) {
                Intent intent = allSticky.get(i);
                BroadcastQueue queue = broadcastQueueForIntent(intent);
                BroadcastRecord r = new BroadcastRecord(queue, intent, null,
                        null, null, -1, -1, false, null, null, OP_NONE, null, receivers,
                        null, 0, null, null, false, true, true, -1, false,
                        false /* only PRE_BOOT_COMPLETED should be exempt, no stickies */);
                queue.enqueueParallelBroadcastLocked(r);
                queue.scheduleBroadcastsLocked();
            }
        }

        return sticky; // TODO return均为sticky，需要查阅文档
    }
}




    /**
     * State of all active sticky broadcasts per user.  Keys are the action of the
     * sticky Intent, values are an ArrayList of all broadcasted intents with
     * that action (which should usually be one).  The SparseArray is keyed
     * by the user ID the sticky is for, and can include UserHandle.USER_ALL
     * for stickies that are sent to all users.
     */
    final SparseArray<ArrayMap<String, ArrayList<Intent>>> mStickyBroadcasts =
            new SparseArray<ArrayMap<String, ArrayList<Intent>>>();
    // KV形式是：K = action, v = 对应action的broadcast intents  
