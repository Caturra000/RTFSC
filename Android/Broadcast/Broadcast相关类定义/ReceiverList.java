
final class ReceiverList extends ArrayList<BroadcastFilter> { // implements IBinder.DeathRecipient

    final ActivityManagerService owner;
    public final IIntentReceiver receiver;
    public final ProcessRecord app;
    public final int pid;
    public final int uid;
    public final int userId;
    BroadcastRecord curBroadcast = null;


    public boolean containsFilter(IntentFilter filter) {
        final int N = size();
        for(int i = 0; i < N; ++i) {
            final BroadcastFilter f = get(i);
            if(IntentResolver.filterEquals(f, filter)) {
                return true;
            }
        }
        return false;
    }

    // dump和writeToProto
}