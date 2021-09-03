// 文件：frameworks/base/services/java/com/android/server/SystemServer.java
// 版本：android-11.0.0_r40

public final class SystemServer {

    // ...

    private SystemServiceManager mSystemServiceManager;

    // ...

    /**
     * The main entry point from zygote.
     */
    public static void main(String[] args) {
        new SystemServer().run();
    }

    // 官方注释已经非常详尽了
    // ##flag #0前都是一些常规处理
    // ##flag #1和##flag #2涉及到Manager
    private void run() {
        TimingsTraceAndSlog t = new TimingsTraceAndSlog();
        try {
            t.traceBegin("InitBeforeStartServices");

            // Record the process start information in sys props.
            SystemProperties.set(SYSPROP_START_COUNT, String.valueOf(mStartCount));
            SystemProperties.set(SYSPROP_START_ELAPSED, String.valueOf(mRuntimeStartElapsedTime));
            SystemProperties.set(SYSPROP_START_UPTIME, String.valueOf(mRuntimeStartUptime));

            EventLog.writeEvent(EventLogTags.SYSTEM_SERVER_START,
                    mStartCount, mRuntimeStartUptime, mRuntimeStartElapsedTime);

            //
            // Default the timezone property to GMT if not set.
            //
            String timezoneProperty = SystemProperties.get("persist.sys.timezone");
            if (timezoneProperty == null || timezoneProperty.isEmpty()) {
                Slog.w(TAG, "Timezone not set; setting to GMT.");
                SystemProperties.set("persist.sys.timezone", "GMT");
            }

            // If the system has "persist.sys.language" and friends set, replace them with
            // "persist.sys.locale". Note that the default locale at this point is calculated
            // using the "-Duser.locale" command line flag. That flag is usually populated by
            // AndroidRuntime using the same set of system properties, but only the system_server
            // and system apps are allowed to set them.
            //
            // NOTE: Most changes made here will need an equivalent change to
            // core/jni/AndroidRuntime.cpp
            if (!SystemProperties.get("persist.sys.language").isEmpty()) {
                final String languageTag = Locale.getDefault().toLanguageTag();

                SystemProperties.set("persist.sys.locale", languageTag);
                SystemProperties.set("persist.sys.language", "");
                SystemProperties.set("persist.sys.country", "");
                SystemProperties.set("persist.sys.localevar", "");
            }

            // The system server should never make non-oneway calls
            Binder.setWarnOnBlocking(true);
            // The system server should always load safe labels
            PackageItemInfo.forceSafeLabels();

            // Default to FULL within the system server.
            SQLiteGlobal.sDefaultSyncMode = SQLiteGlobal.SYNC_MODE_FULL;

            // Deactivate SQLiteCompatibilityWalFlags until settings provider is initialized
            SQLiteCompatibilityWalFlags.init(null);

            // Here we go!
            // ##flag #0
            Slog.i(TAG, "Entered the Android system server!");
            final long uptimeMillis = SystemClock.elapsedRealtime();
            EventLog.writeEvent(EventLogTags.BOOT_PROGRESS_SYSTEM_RUN, uptimeMillis);
            if (!mRuntimeRestart) {
                FrameworkStatsLog.write(FrameworkStatsLog.BOOT_TIME_EVENT_ELAPSED_TIME_REPORTED,
                        FrameworkStatsLog
                                .BOOT_TIME_EVENT_ELAPSED_TIME__EVENT__SYSTEM_SERVER_INIT_START,
                        uptimeMillis);
            }

            // In case the runtime switched since last boot (such as when
            // the old runtime was removed in an OTA), set the system
            // property so that it is in sync. We can't do this in
            // libnativehelper's JniInvocation::Init code where we already
            // had to fallback to a different runtime because it is
            // running as root and we need to be the system user to set
            // the property. http://b/11463182
            SystemProperties.set("persist.sys.dalvik.vm.lib.2", VMRuntime.getRuntime().vmLibrary());

            // Mmmmmm... more memory!
            VMRuntime.getRuntime().clearGrowthLimit();

            // Some devices rely on runtime fingerprint generation, so make sure
            // we've defined it before booting further.
            Build.ensureFingerprintProperty();

            // Within the system server, it is an error to access Environment paths without
            // explicitly specifying a user.
            Environment.setUserRequired(true);

            // Within the system server, any incoming Bundles should be defused
            // to avoid throwing BadParcelableException.
            BaseBundle.setShouldDefuse(true);

            // Within the system server, when parceling exceptions, include the stack trace
            Parcel.setStackTraceParceling(true);

            // Ensure binder calls into the system always run at foreground priority.
            BinderInternal.disableBackgroundScheduling(true);

            // Increase the number of binder threads in system_server
            BinderInternal.setMaxThreads(sMaxBinderThreads);

            // Prepare the main looper thread (this thread).
            android.os.Process.setThreadPriority(
                    android.os.Process.THREAD_PRIORITY_FOREGROUND);
            android.os.Process.setCanSelfBackground(false);
            Looper.prepareMainLooper();
            Looper.getMainLooper().setSlowLogThresholdMs(
                    SLOW_DISPATCH_THRESHOLD_MS, SLOW_DELIVERY_THRESHOLD_MS);

            SystemServiceRegistry.sEnableServiceNotFoundWtf = true;

            // Initialize native services.
            System.loadLibrary("android_servers");

            // Allow heap / perf profiling.
            initZygoteChildHeapProfiling();

            // Debug builds - spawn a thread to monitor for fd leaks.
            if (Build.IS_DEBUGGABLE) {
                spawnFdLeakCheckThread();
            }

            // Check whether we failed to shut down last time we tried.
            // This call may not return.
            performPendingShutdown();

            // Initialize the system context.
            createSystemContext();

            // Call per-process mainline module initialization.
            ActivityThread.initializeMainlineModules();

            // Create the system service manager.
            // ##flag #1
            mSystemServiceManager = new SystemServiceManager(mSystemContext);
            mSystemServiceManager.setStartInfo(mRuntimeRestart,
                    mRuntimeStartElapsedTime, mRuntimeStartUptime);
            LocalServices.addService(SystemServiceManager.class, mSystemServiceManager);
            // Prepare the thread pool for init tasks that can be parallelized
            SystemServerInitThreadPool.start();
            // Attach JVMTI agent if this is a debuggable build and the system property is set.
            if (Build.IS_DEBUGGABLE) {
                // Property is of the form "library_path=parameters".
                String jvmtiAgent = SystemProperties.get("persist.sys.dalvik.jvmtiagent");
                if (!jvmtiAgent.isEmpty()) {
                    int equalIndex = jvmtiAgent.indexOf('=');
                    String libraryPath = jvmtiAgent.substring(0, equalIndex);
                    String parameterList =
                            jvmtiAgent.substring(equalIndex + 1, jvmtiAgent.length());
                    // Attach the agent.
                    try {
                        Debug.attachJvmtiAgent(libraryPath, parameterList, null);
                    } catch (Exception e) {
                        Slog.e("System", "*************************************************");
                        Slog.e("System", "********** Failed to load jvmti plugin: " + jvmtiAgent);
                    }
                }
            }
        } finally {
            t.traceEnd();  // InitBeforeStartServices
        }

        // Setup the default WTF handler
        RuntimeInit.setDefaultApplicationWtfHandler(SystemServer::handleEarlySystemWtf);

        // Start services.
        // ##flag #2
        try {
            t.traceBegin("StartServices");
            startBootstrapServices(t);
            startCoreServices(t);
            startOtherServices(t);
        } catch (Throwable ex) {
            Slog.e("System", "******************************************");
            Slog.e("System", "************ Failure starting system services", ex);
            throw ex;
        } finally {
            t.traceEnd(); // StartServices
        }

        StrictMode.initVmDefaults(null);

        if (!mRuntimeRestart && !isFirstBootOrUpgrade()) {
            final long uptimeMillis = SystemClock.elapsedRealtime();
            FrameworkStatsLog.write(FrameworkStatsLog.BOOT_TIME_EVENT_ELAPSED_TIME_REPORTED,
                    FrameworkStatsLog.BOOT_TIME_EVENT_ELAPSED_TIME__EVENT__SYSTEM_SERVER_READY,
                    uptimeMillis);
            final long maxUptimeMillis = 60 * 1000;
            if (uptimeMillis > maxUptimeMillis) {
                Slog.wtf(SYSTEM_SERVER_TIMING_TAG,
                        "SystemServer init took too long. uptimeMillis=" + uptimeMillis);
            }
        }

        // Diagnostic to ensure that the system is in a base healthy state. Done here as a common
        // non-zygote process.
        if (!VMRuntime.hasBootImageSpaces()) {
            Slog.wtf(TAG, "Runtime is not running with a boot image!");
        }

        // Loop forever.
        Looper.loop();
        throw new RuntimeException("Main thread loop unexpectedly exited");
    }









    /**
     * Starts the small tangle of critical services that are needed to get the system off the
     * ground.  These services have complex mutual dependencies which is why we initialize them all
     * in one place here.  Unless your service is also entwined in these dependencies, it should be
     * initialized in one of the other functions.
     */
    private void startBootstrapServices(@NonNull TimingsTraceAndSlog t) {
        t.traceBegin("startBootstrapServices");

        // Start the watchdog as early as possible so we can crash the system server
        // if we deadlock during early boot
        t.traceBegin("StartWatchdog");
        final Watchdog watchdog = Watchdog.getInstance();
        watchdog.start();
        t.traceEnd();

        Slog.i(TAG, "Reading configuration...");
        final String TAG_SYSTEM_CONFIG = "ReadingSystemConfig";
        t.traceBegin(TAG_SYSTEM_CONFIG);
        SystemServerInitThreadPool.submit(SystemConfig::getInstance, TAG_SYSTEM_CONFIG);
        t.traceEnd();

        // Platform compat service is used by ActivityManagerService, PackageManagerService, and
        // possibly others in the future. b/135010838.
        t.traceBegin("PlatformCompat");
        PlatformCompat platformCompat = new PlatformCompat(mSystemContext);
        ServiceManager.addService(Context.PLATFORM_COMPAT_SERVICE, platformCompat);
        ServiceManager.addService(Context.PLATFORM_COMPAT_NATIVE_SERVICE,
                new PlatformCompatNative(platformCompat));
        AppCompatCallbacks.install(new long[0]);
        t.traceEnd();

        // FileIntegrityService responds to requests from apps and the system. It needs to run after
        // the source (i.e. keystore) is ready, and before the apps (or the first customer in the
        // system) run.
        t.traceBegin("StartFileIntegrityService");
        mSystemServiceManager.startService(FileIntegrityService.class);
        t.traceEnd();

        // Wait for installd to finish starting up so that it has a chance to
        // create critical directories such as /data/user with the appropriate
        // permissions.  We need this to complete before we initialize other services.
        t.traceBegin("StartInstaller");
        Installer installer = mSystemServiceManager.startService(Installer.class);
        t.traceEnd();

        // In some cases after launching an app we need to access device identifiers,
        // therefore register the device identifier policy before the activity manager.
        t.traceBegin("DeviceIdentifiersPolicyService");
        mSystemServiceManager.startService(DeviceIdentifiersPolicyService.class);
        t.traceEnd();

        // Uri Grants Manager.
        t.traceBegin("UriGrantsManagerService");
        mSystemServiceManager.startService(UriGrantsManagerService.Lifecycle.class);
        t.traceEnd();

        // Activity manager runs the show.
        t.traceBegin("StartActivityManager");
        // TODO: Might need to move after migration to WM.
        ActivityTaskManagerService atm = mSystemServiceManager.startService(
                ActivityTaskManagerService.Lifecycle.class).getService();
        mActivityManagerService = ActivityManagerService.Lifecycle.startService(
                mSystemServiceManager, atm);
        mActivityManagerService.setSystemServiceManager(mSystemServiceManager);
        mActivityManagerService.setInstaller(installer);
        mWindowManagerGlobalLock = atm.getGlobalLock();
        t.traceEnd();

        // Data loader manager service needs to be started before package manager
        t.traceBegin("StartDataLoaderManagerService");
        mDataLoaderManagerService = mSystemServiceManager.startService(
                DataLoaderManagerService.class);
        t.traceEnd();

        // Incremental service needs to be started before package manager
        t.traceBegin("StartIncrementalService");
        mIncrementalServiceHandle = startIncrementalService();
        t.traceEnd();

        // Power manager needs to be started early because other services need it.
        // Native daemons may be watching for it to be registered so it must be ready
        // to handle incoming binder calls immediately (including being able to verify
        // the permissions for those calls).
        t.traceBegin("StartPowerManager");
        mPowerManagerService = mSystemServiceManager.startService(PowerManagerService.class);
        t.traceEnd();

        t.traceBegin("StartThermalManager");
        mSystemServiceManager.startService(ThermalManagerService.class);
        t.traceEnd();

        // Now that the power manager has been started, let the activity manager
        // initialize power management features.
        t.traceBegin("InitPowerManagement");
        mActivityManagerService.initPowerManagement();
        t.traceEnd();

        // Bring up recovery system in case a rescue party needs a reboot
        t.traceBegin("StartRecoverySystemService");
        mSystemServiceManager.startService(RecoverySystemService.Lifecycle.class);
        t.traceEnd();

        // Now that we have the bare essentials of the OS up and running, take
        // note that we just booted, which might send out a rescue party if
        // we're stuck in a runtime restart loop.
        RescueParty.registerHealthObserver(mSystemContext);
        PackageWatchdog.getInstance(mSystemContext).noteBoot();

        // Manages LEDs and display backlight so we need it to bring up the display.
        t.traceBegin("StartLightsService");
        mSystemServiceManager.startService(LightsService.class);
        t.traceEnd();

        t.traceBegin("StartSidekickService");
        // Package manager isn't started yet; need to use SysProp not hardware feature
        if (SystemProperties.getBoolean("config.enable_sidekick_graphics", false)) {
            mSystemServiceManager.startService(WEAR_SIDEKICK_SERVICE_CLASS);
        }
        t.traceEnd();

        // Display manager is needed to provide display metrics before package manager
        // starts up.
        t.traceBegin("StartDisplayManager");
        mDisplayManagerService = mSystemServiceManager.startService(DisplayManagerService.class);
        t.traceEnd();

        // We need the default display before we can initialize the package manager.
        t.traceBegin("WaitForDisplay");
        mSystemServiceManager.startBootPhase(t, SystemService.PHASE_WAIT_FOR_DEFAULT_DISPLAY);
        t.traceEnd();

        // Only run "core" apps if we're encrypting the device.
        String cryptState = VoldProperties.decrypt().orElse("");
        if (ENCRYPTING_STATE.equals(cryptState)) {
            Slog.w(TAG, "Detected encryption in progress - only parsing core apps");
            mOnlyCore = true;
        } else if (ENCRYPTED_STATE.equals(cryptState)) {
            Slog.w(TAG, "Device encrypted - only parsing core apps");
            mOnlyCore = true;
        }

        // Start the package manager.
        if (!mRuntimeRestart) {
            FrameworkStatsLog.write(FrameworkStatsLog.BOOT_TIME_EVENT_ELAPSED_TIME_REPORTED,
                    FrameworkStatsLog
                            .BOOT_TIME_EVENT_ELAPSED_TIME__EVENT__PACKAGE_MANAGER_INIT_START,
                    SystemClock.elapsedRealtime());
        }

        t.traceBegin("StartPackageManagerService");
        try {
            Watchdog.getInstance().pauseWatchingCurrentThread("packagemanagermain");
            mPackageManagerService = PackageManagerService.main(mSystemContext, installer,
                    mFactoryTestMode != FactoryTest.FACTORY_TEST_OFF, mOnlyCore);
        } finally {
            Watchdog.getInstance().resumeWatchingCurrentThread("packagemanagermain");
        }

        // Now that the package manager has started, register the dex load reporter to capture any
        // dex files loaded by system server.
        // These dex files will be optimized by the BackgroundDexOptService.
        SystemServerDexLoadReporter.configureSystemServerDexReporter(mPackageManagerService);

        mFirstBoot = mPackageManagerService.isFirstBoot();
        mPackageManager = mSystemContext.getPackageManager();
        t.traceEnd();
        if (!mRuntimeRestart && !isFirstBootOrUpgrade()) {
            FrameworkStatsLog.write(FrameworkStatsLog.BOOT_TIME_EVENT_ELAPSED_TIME_REPORTED,
                    FrameworkStatsLog
                            .BOOT_TIME_EVENT_ELAPSED_TIME__EVENT__PACKAGE_MANAGER_INIT_READY,
                    SystemClock.elapsedRealtime());
        }
        // Manages A/B OTA dexopting. This is a bootstrap service as we need it to rename
        // A/B artifacts after boot, before anything else might touch/need them.
        // Note: this isn't needed during decryption (we don't have /data anyways).
        if (!mOnlyCore) {
            boolean disableOtaDexopt = SystemProperties.getBoolean("config.disable_otadexopt",
                    false);
            if (!disableOtaDexopt) {
                t.traceBegin("StartOtaDexOptService");
                try {
                    Watchdog.getInstance().pauseWatchingCurrentThread("moveab");
                    OtaDexoptService.main(mSystemContext, mPackageManagerService);
                } catch (Throwable e) {
                    reportWtf("starting OtaDexOptService", e);
                } finally {
                    Watchdog.getInstance().resumeWatchingCurrentThread("moveab");
                    t.traceEnd();
                }
            }
        }

        t.traceBegin("StartUserManagerService");
        mSystemServiceManager.startService(UserManagerService.LifeCycle.class);
        t.traceEnd();

        // Initialize attribute cache used to cache resources from packages.
        t.traceBegin("InitAttributerCache");
        AttributeCache.init(mSystemContext);
        t.traceEnd();

        // Set up the Application instance for the system process and get started.
        t.traceBegin("SetSystemProcess");
        mActivityManagerService.setSystemProcess();
        t.traceEnd();

        // Complete the watchdog setup with an ActivityManager instance and listen for reboots
        // Do this only after the ActivityManagerService is properly started as a system process
        t.traceBegin("InitWatchdog");
        watchdog.init(mSystemContext, mActivityManagerService);
        t.traceEnd();

        // DisplayManagerService needs to setup android.display scheduling related policies
        // since setSystemProcess() would have overridden policies due to setProcessGroup
        mDisplayManagerService.setupSchedulerPolicies();

        // Manages Overlay packages
        t.traceBegin("StartOverlayManagerService");
        mSystemServiceManager.startService(new OverlayManagerService(mSystemContext));
        t.traceEnd();

        t.traceBegin("StartSensorPrivacyService");
        mSystemServiceManager.startService(new SensorPrivacyService(mSystemContext));
        t.traceEnd();

        if (SystemProperties.getInt("persist.sys.displayinset.top", 0) > 0) {
            // DisplayManager needs the overlay immediately.
            mActivityManagerService.updateSystemUiContext();
            LocalServices.getService(DisplayManagerInternal.class).onOverlayChanged();
        }

        // The sensor service needs access to package manager service, app ops
        // service, and permissions service, therefore we start it after them.
        // Start sensor service in a separate thread. Completion should be checked
        // before using it.
        mSensorServiceStart = SystemServerInitThreadPool.submit(() -> {
            TimingsTraceAndSlog traceLog = TimingsTraceAndSlog.newAsyncLog();
            traceLog.traceBegin(START_SENSOR_SERVICE);
            startSensorService();
            traceLog.traceEnd();
        }, START_SENSOR_SERVICE);

        t.traceEnd(); // startBootstrapServices
    }












    /**
     * Starts some essential services that are not tangled up in the bootstrap process.
     */
    private void startCoreServices(@NonNull TimingsTraceAndSlog t) {
        t.traceBegin("startCoreServices");

        // Service for system config
        t.traceBegin("StartSystemConfigService");
        mSystemServiceManager.startService(SystemConfigService.class);
        t.traceEnd();

        t.traceBegin("StartBatteryService");
        // Tracks the battery level.  Requires LightService.
        mSystemServiceManager.startService(BatteryService.class);
        t.traceEnd();

        // Tracks application usage stats.
        t.traceBegin("StartUsageService");
        mSystemServiceManager.startService(UsageStatsService.class);
        mActivityManagerService.setUsageStatsManager(
                LocalServices.getService(UsageStatsManagerInternal.class));
        t.traceEnd();

        // Tracks whether the updatable WebView is in a ready state and watches for update installs.
        if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_WEBVIEW)) {
            t.traceBegin("StartWebViewUpdateService");
            mWebViewUpdateService = mSystemServiceManager.startService(WebViewUpdateService.class);
            t.traceEnd();
        }

        // Tracks and caches the device state.
        t.traceBegin("StartCachedDeviceStateService");
        mSystemServiceManager.startService(CachedDeviceStateService.class);
        t.traceEnd();

        // Tracks cpu time spent in binder calls
        t.traceBegin("StartBinderCallsStatsService");
        mSystemServiceManager.startService(BinderCallsStatsService.LifeCycle.class);
        t.traceEnd();

        // Tracks time spent in handling messages in handlers.
        t.traceBegin("StartLooperStatsService");
        mSystemServiceManager.startService(LooperStatsService.Lifecycle.class);
        t.traceEnd();

        // Manages apk rollbacks.
        t.traceBegin("StartRollbackManagerService");
        mSystemServiceManager.startService(ROLLBACK_MANAGER_SERVICE_CLASS);
        t.traceEnd();

        // Service to capture bugreports.
        t.traceBegin("StartBugreportManagerService");
        mSystemServiceManager.startService(BugreportManagerService.class);
        t.traceEnd();

        // Serivce for GPU and GPU driver.
        t.traceBegin("GpuService");
        mSystemServiceManager.startService(GpuService.class);
        t.traceEnd();

        t.traceEnd(); // startCoreServices
    }

}