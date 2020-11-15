// 文件分布：frameworks/base/services/core/java/com/android/server/wm/ActivityRecord.java

// TODO

// Draft
// 在execute()过程中，ActivityRecord是作为caller调用的

/**
 * An entry in the history stack, representing an activity.
 */
final class ActivityRecord extends WindowToken implements WindowManagerService.AppFreezeListener {
