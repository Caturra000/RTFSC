// 文件分布：frameworks/base/services/core/java/com/android/server/wm/ActivityRecord.java

// TODO

// Draft
// 在execute()过程中，ActivityRecord是作为caller调用的

/**
 * An entry in the history stack, representing an activity.
 */
final class ActivityRecord extends WindowToken implements WindowManagerService.AppFreezeListener {


    ArrayList<ResultInfo> results; // pending ActivityResult objs we have received

    // 删除完全匹配from resultWho requestCode的resultInfo
    void removeResultsLocked(ActivityRecord from, String resultWho,
            int requestCode) {
        if (results != null) {
            for (int i=results.size()-1; i>=0; i--) {
                ActivityResult r = (ActivityResult)results.get(i);
                if (r.mFrom != from) continue;
                // 必须匹配from
                if (r.mResultWho == null) {
                    if (resultWho != null) continue; // mResultWho和resultWho为null的话就能匹配
                } else {
                    if (!r.mResultWho.equals(resultWho)) continue;
                }
                if (r.mRequestCode != requestCode) continue;

                results.remove(i);
            }
        }
    }

}