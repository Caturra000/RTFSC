
    // TODO

    // 文件分布：frameworks/base/services/core/java/com/android/server/wm/Task.java

    // public class ActivityStack extends Task

    class Task extends WindowContainer<WindowContainer> {
        private static final String TAG = TAG_WITH_CLASS_NAME ? "Task" : TAG_ATM;
        private static final String TAG_ADD_REMOVE = TAG + POSTFIX_ADD_REMOVE;
        private static final String TAG_RECENTS = TAG + POSTFIX_RECENTS;
        private static final String TAG_LOCKTASK = TAG + POSTFIX_LOCKTASK;
        private static final String TAG_TASKS = TAG + POSTFIX_TASKS;
    
        private static final String ATTR_TASKID = "task_id";
        private static final String TAG_INTENT = "intent";
        private static final String TAG_AFFINITYINTENT = "affinity_intent";
        private static final String ATTR_REALACTIVITY = "real_activity";
        private static final String ATTR_REALACTIVITY_SUSPENDED = "real_activity_suspended";
        private static final String ATTR_ORIGACTIVITY = "orig_activity";
        private static final String TAG_ACTIVITY = "activity";
        private static final String ATTR_AFFINITY = "affinity";
        private static final String ATTR_ROOT_AFFINITY = "root_affinity";
        private static final String ATTR_ROOTHASRESET = "root_has_reset";
        private static final String ATTR_AUTOREMOVERECENTS = "auto_remove_recents";
        private static final String ATTR_ASKEDCOMPATMODE = "asked_compat_mode";
        private static final String ATTR_USERID = "user_id";
        private static final String ATTR_USER_SETUP_COMPLETE = "user_setup_complete";
        private static final String ATTR_EFFECTIVE_UID = "effective_uid";
        @Deprecated
        private static final String ATTR_TASKTYPE = "task_type";
        private static final String ATTR_LASTDESCRIPTION = "last_description";
        private static final String ATTR_LASTTIMEMOVED = "last_time_moved";
        private static final String ATTR_NEVERRELINQUISH = "never_relinquish_identity";
        private static final String ATTR_TASK_AFFILIATION = "task_affiliation";
        private static final String ATTR_PREV_AFFILIATION = "prev_affiliation";
        private static final String ATTR_NEXT_AFFILIATION = "next_affiliation";
        private static final String ATTR_TASK_AFFILIATION_COLOR = "task_affiliation_color";
        private static final String ATTR_CALLING_UID = "calling_uid";
        private static final String ATTR_CALLING_PACKAGE = "calling_package";
        private static final String ATTR_CALLING_FEATURE_ID = "calling_feature_id";
        private static final String ATTR_SUPPORTS_PICTURE_IN_PICTURE = "supports_picture_in_picture";
        private static final String ATTR_RESIZE_MODE = "resize_mode";
        private static final String ATTR_NON_FULLSCREEN_BOUNDS = "non_fullscreen_bounds";
        private static final String ATTR_MIN_WIDTH = "min_width";
        private static final String ATTR_MIN_HEIGHT = "min_height";
        private static final String ATTR_PERSIST_TASK_VERSION = "persist_task_version";
        private static final String ATTR_WINDOW_LAYOUT_AFFINITY = "window_layout_affinity";

        // .....