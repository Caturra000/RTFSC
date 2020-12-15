// 文件分布：frameworks/base/core/java/android/app/ContextImpl.java

    // 1. 准备receiverDispatcher
    // 2. 调用AMS
    private Intent registerReceiverInternal(BroadcastReceiver receiver, int userId,
            IntentFilter filter, String broadcastPermission,
            Handler scheduler, Context context, int flags) { // scheduler决定在onReceive哪个线程执行
        IIntentReceiver rd = null; // binder对象，表示AMS端的BroadcastReceiver
        // rd有两个来源，一个是mPackageInfo，另一个是LoadedApk                                        // TODO
        if (receiver != null) {
            if (mPackageInfo != null && context != null) {
                if (scheduler == null) {
                    scheduler = mMainThread.getHandler();
                }
                rd = mPackageInfo.getReceiverDispatcher(
                    receiver, context, scheduler,
                    mMainThread.getInstrumentation(), true);
            } else {
                if (scheduler == null) {
                    scheduler = mMainThread.getHandler();
                }
                // 一般关注这个
                rd = new LoadedApk.ReceiverDispatcher(
                        receiver, context, scheduler, null, true).getIIntentReceiver();
            }
        }
        try {
            final Intent intent = ActivityManager.getService().registerReceiverWithFeature(
                    mMainThread.getApplicationThread(), mBasePackageName, getAttributionTag(), rd,    // TODO 什么时候getApplicationThread为null
                    filter, broadcastPermission, userId, flags);
            if (intent != null) {
                intent.setExtrasClassLoader(getClassLoader());
                intent.prepareToEnterProcess();
            }
            return intent; // 返回的是一个sticky intent
        } catch (RemoteException e) {
            throw e.rethrowFromSystemServer();
        }
    }




// 文件分布：ActivityManagerService.java

    // 1. 通过caller处理callingUid/callingPid...
    // 2. 从filter获取actions，迭代并通过mStickyBroadcasts(key: action, value: ArrayList<Intent>)更新stickyIntents
    // 3. 将stickyIntent中的intent进一步筛选(filter.match())写入allSticky
    // 4. 处理ReceiverList和BroadcastFilter
    // 5. 单独处理sticky调度
    public Intent registerReceiverWithFeature(IApplicationThread caller, String callerPackage,
            String callerFeatureId, IIntentReceiver receiver, IntentFilter filter,
            String permission, int userId, int flags) {
        enforceNotIsolatedCaller("registerReceiver");
        ArrayList<Intent> stickyIntents = null;
        ProcessRecord callerApp = null;
        final boolean visibleToInstantApps
                = (flags & Context.RECEIVER_VISIBLE_TO_INSTANT_APPS) != 0;
        int callingUid;
        int callingPid;
        boolean instantApp;
        synchronized(this) {
            if (caller != null) {
                callerApp = getRecordForAppLocked(caller); // 从LRU-list中获取
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
            } else {                                                                               // TODO caller == null
                callerPackage = null;
                callingUid = Binder.getCallingUid();
                callingPid = Binder.getCallingPid();
            }

            instantApp = isInstantApp(callerApp, callerPackage, callingUid);
            userId = mUserController.handleIncomingUser(callingPid, callingUid, userId, true,
                    ALLOW_FULL_ONLY, "registerReceiver", callerPackage);

            Iterator<String> actions = filter.actionsIterator();
            if (actions == null) {
                ArrayList<String> noAction = new ArrayList<String>(1);
                noAction.add(null);
                actions = noAction.iterator();
            }

// SKIP START
            // 见sticky处理.java
            // Collect stickies of users
            int[] userIds = { UserHandle.USER_ALL, UserHandle.getUserId(callingUid) };
            while (actions.hasNext()) {
                String action = actions.next();
                for (int id : userIds) {
                    ArrayMap<String, ArrayList<Intent>> stickies = mStickyBroadcasts.get(id);
                    if (stickies != null) {
                        ArrayList<Intent> intents = stickies.get(action);
                        if (intents != null) {
                            if (stickyIntents == null) {
                                stickyIntents = new ArrayList<Intent>();
                            }
                            stickyIntents.addAll(intents);
                        }
                    }
                }
            }
        }

        ArrayList<Intent> allSticky = null;
        if (stickyIntents != null) {
            final ContentResolver resolver = mContext.getContentResolver();
            // Look for any matching sticky broadcasts...
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
                if (filter.match(resolver, intent, true, TAG) >= 0) {
                    if (allSticky == null) {
                        allSticky = new ArrayList<Intent>();
                    }
                    allSticky.add(intent);
                }
            }
        }

        // The first sticky in the list is returned directly back to the client.
        Intent sticky = allSticky != null ? allSticky.get(0) : null;
        if (DEBUG_BROADCAST) Slog.v(TAG_BROADCAST, "Register receiver " + filter + ": " + sticky);
        if (receiver == null) {
            return sticky;
        }
// SKIP END
        synchronized (this) {
            if (callerApp != null && (callerApp.thread == null
                    || callerApp.thread.asBinder() != caller.asBinder())) {
                // Original caller already died
                return null;
            }
            // ReceiverList是指多个filter，单个receiver（binder）
            ReceiverList rl = mRegisteredReceivers.get(receiver.asBinder());
            // 首次
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
                    rl.app.receivers.add(rl);  // ProcessRecord.receivers.add
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
                    // 这里只为sticky单独调度，发送一个msg
                    queue.enqueueParallelBroadcastLocked(r);
                    queue.scheduleBroadcastsLocked();
                }
            }

            return sticky;
        }
    }
