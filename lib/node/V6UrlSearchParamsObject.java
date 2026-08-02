import java.util.ArrayList;
import java.util.List;

public final class V6UrlSearchParamsObject extends V6Object {
  final List<String[]> pairs = new ArrayList<>();
  Runnable onChange;

  void notifyChange() {
    if (onChange != null)
      onChange.run();
  }
}
