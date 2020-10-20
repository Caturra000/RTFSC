// Google注释提到是Structured description of Intent values to be matched.
public class IntentFilter {

    // 很庞大的类，节选

    // 匹配发起的Intent是否匹配成功，action/type/data/category任意unmatch都会失败
    // Test whether this filter matches the given intent，注意intent就是tested对象
    // TODO resolver没提到怎么用
    public final int match(ContentResolver resolver,
                            Intent intent, boolean resolve, String logTag) {
            // 实际调用match(String action, String type, String scheme, Uri data, Set<String> categories, String logTag)
            // Filter本身持有一些Intent匹配用的成员，内部会分别matchAction(action) / matchData(type, schema, data) / matchCategories
    }

}