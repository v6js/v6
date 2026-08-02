import java.time.Instant;
import java.time.LocalDate;
import java.time.LocalDateTime;
import java.time.ZoneId;
import java.time.ZoneOffset;
import java.time.ZonedDateTime;
import java.time.format.DateTimeFormatter;
import java.time.format.DateTimeParseException;
import java.util.Locale;

public final class V6DateConstructor
    extends V6Object implements V6NativeConstructor {
  public static final V6Object PROTOTYPE = buildPrototype();

  private static final DateTimeFormatter TO_STRING_FMT =
      DateTimeFormatter.ofPattern("EEE MMM dd yyyy HH:mm:ss 'GMT'xx",
                                  Locale.US);
  private static final DateTimeFormatter DATE_STRING_FMT =
      DateTimeFormatter.ofPattern("EEE MMM dd yyyy", Locale.US);
  private static final DateTimeFormatter TIME_STRING_FMT =
      DateTimeFormatter.ofPattern("HH:mm:ss 'GMT'xx", Locale.US);
  private static final DateTimeFormatter UTC_STRING_FMT =
      DateTimeFormatter.ofPattern("EEE, dd MMM yyyy HH:mm:ss 'GMT'", Locale.US);
  private static final DateTimeFormatter ISO_FMT =
      DateTimeFormatter.ofPattern("yyyy-MM-dd'T'HH:mm:ss.SSS'Z'", Locale.US);

  public V6DateConstructor() {
    set("prototype", new V6Value(V6Value.TAG_OBJ, 0, PROTOTYPE));
    PROTOTYPE.nativeCtor = this;
    set("now", fn((t, a) -> num(System.currentTimeMillis())));
    set("parse",
        fn((t, a) -> num(parseToMillis(V6Value.argAt(a, 0).toString()))));
    set("UTC", fn((t, a) -> num(utcFromArgs(a))));
  }

  @Override
  public V6Object allocate() {
    return new V6DateObject(Double.NaN);
  }

  @Override
  public V6Value construct(V6Value[] args) {
    V6DateObject d = new V6DateObject(0);
    d.setProto(PROTOTYPE);
    initInstance(d, args);
    return new V6Value(V6Value.TAG_OBJ, 0, d);
  }

  @Override
  public void initInstance(V6Object instance, V6Value[] args) {
    ((V6DateObject)instance).epochMillis =
        computeMillis(args, ZoneId.systemDefault());
  }

  @Override
  public V6Object prototypeObject() {
    return PROTOTYPE;
  }

  private static V6Value str(String s) {
    return new V6Value(V6Value.TAG_STR, 0, s);
  }

  private static V6Value fn(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  private static V6Value num(double n) {
    return new V6Value(V6Value.TAG_NUM, n, null);
  }

  private static V6DateObject self(V6Value t) {
    return (V6DateObject)t.ref();
  }

  private static double buildMillis(int year, int month, int day, int hours,
                                    int minutes, int seconds, int millis,
                                    ZoneId zone) {
    try {
      if (year >= 0 && year <= 99)
        year += 1900;
      LocalDate base =
          LocalDate.of(year, 1, 1).plusMonths(month).plusDays(day - 1L);
      LocalDateTime ldt = base.atStartOfDay()
                              .plusHours(hours)
                              .plusMinutes(minutes)
                              .plusSeconds(seconds)
                              .plusNanos(millis * 1_000_000L);
      return ldt.atZone(zone).toInstant().toEpochMilli();
    } catch (Exception e) {
      return Double.NaN;
    }
  }

  private static double computeMillis(V6Value[] args, ZoneId zone) {
    if (args.length == 0)
      return System.currentTimeMillis();
    if (args.length == 1) {
      V6Value a0 = args[0];
      if (a0.tag() == V6Value.TAG_STR)
        return parseToMillis(a0.toString());
      if (a0.tag() == V6Value.TAG_OBJ && a0.ref() instanceof V6DateObject)
        return ((V6DateObject)a0.ref()).epochMillis;
      return a0.toNumber();
    }
    int year = (int)args[0].toNumber();
    int month = (int)V6Value.argAt(args, 1).toNumber();
    int day = args.length > 2 ? (int)args[2].toNumber() : 1;
    int hours = args.length > 3 ? (int)args[3].toNumber() : 0;
    int minutes = args.length > 4 ? (int)args[4].toNumber() : 0;
    int seconds = args.length > 5 ? (int)args[5].toNumber() : 0;
    int millis = args.length > 6 ? (int)args[6].toNumber() : 0;
    return buildMillis(year, month, day, hours, minutes, seconds, millis, zone);
  }

  private static double utcFromArgs(V6Value[] args) {
    if (args.length == 0)
      return Double.NaN;
    int year = (int)args[0].toNumber();
    int month = (int)V6Value.argAt(args, 1).toNumber();
    int day = args.length > 2 ? (int)args[2].toNumber() : 1;
    int hours = args.length > 3 ? (int)args[3].toNumber() : 0;
    int minutes = args.length > 4 ? (int)args[4].toNumber() : 0;
    int seconds = args.length > 5 ? (int)args[5].toNumber() : 0;
    int millis = args.length > 6 ? (int)args[6].toNumber() : 0;
    return buildMillis(year, month, day, hours, minutes, seconds, millis,
                       ZoneOffset.UTC);
  }

  private static double parseToMillis(String s) {
    s = s.trim();
    try {
      return Instant.parse(s).toEpochMilli();
    } catch (DateTimeParseException ignored) {
    }
    String[] patterns = {
        "yyyy-MM-dd'T'HH:mm:ss.SSSXXX",
        "yyyy-MM-dd'T'HH:mm:ssXXX",
        "yyyy-MM-dd'T'HH:mm:ss.SSS",
        "yyyy-MM-dd'T'HH:mm:ss",
        "yyyy-MM-dd'T'HH:mm",
        "yyyy-MM-dd",
        "EEE MMM dd yyyy HH:mm:ss 'GMT'xx",
        "EEE, dd MMM yyyy HH:mm:ss 'GMT'",
        "EEE MMM dd yyyy",
    };
    for (String p : patterns) {
      try {
        DateTimeFormatter fmt = DateTimeFormatter.ofPattern(p, Locale.US);
        if (p.contains("X") || p.contains("GMT'x")) {
          return java.time.OffsetDateTime.parse(s, fmt)
              .toInstant()
              .toEpochMilli();
        } else if (p.equals("yyyy-MM-dd") || p.equals("EEE MMM dd yyyy") ||
                   p.equals("EEE, dd MMM yyyy HH:mm:ss 'GMT'")) {
          if (p.equals("EEE, dd MMM yyyy HH:mm:ss 'GMT'")) {
            LocalDateTime ldt = LocalDateTime.parse(s, fmt);
            return ldt.atZone(ZoneOffset.UTC).toInstant().toEpochMilli();
          }
          LocalDate ld = LocalDate.parse(s, fmt);
          return ld.atStartOfDay(ZoneOffset.UTC).toInstant().toEpochMilli();
        } else {
          LocalDateTime ldt = LocalDateTime.parse(s, fmt);
          return ldt.atZone(ZoneId.systemDefault()).toInstant().toEpochMilli();
        }
      } catch (Exception ignored) {
      }
    }
    return Double.NaN;
  }

  private static ZonedDateTime toZoned(double epochMillis, ZoneId zone) {
    return Instant.ofEpochMilli((long)epochMillis).atZone(zone);
  }

  private static V6Value setFromComponents(V6DateObject d, ZoneId zone,
                                           V6Value[] args, int startField) {
    if (Double.isNaN(d.epochMillis) && args.length == 0)
      return num(Double.NaN);
    ZonedDateTime cur = Double.isNaN(d.epochMillis)
                            ? Instant.EPOCH.atZone(zone)
                            : toZoned(d.epochMillis, zone);
    int[] fields = {cur.getYear(),
                    cur.getMonthValue() - 1,
                    cur.getDayOfMonth(),
                    cur.getHour(),
                    cur.getMinute(),
                    cur.getSecond(),
                    cur.getNano() / 1_000_000};
    for (int i = 0; i < args.length && startField + i < 7; i++)
      fields[startField + i] = (int)args[i].toNumber();
    d.epochMillis = buildMillis(fields[0], fields[1], fields[2], fields[3],
                                fields[4], fields[5], fields[6], zone);
    return num(d.epochMillis);
  }

  private static V6Object buildPrototype() {
    V6Object o = new V6Object();

    o.set("getTime", fn((t, a) -> num(self(t).epochMillis)));
    o.set("valueOf", fn((t, a) -> num(self(t).epochMillis)));
    o.set("setTime", fn((t, a) -> {
            self(t).epochMillis = V6Value.argAt(a, 0).toNumber();
            return num(self(t).epochMillis);
          }));

    o.set("getFullYear",
          fn((t, a)
                 -> num(toZoned(self(t).epochMillis, ZoneId.systemDefault())
                            .getYear())));
    o.set("getMonth",
          fn((t, a)
                 -> num(toZoned(self(t).epochMillis, ZoneId.systemDefault())
                            .getMonthValue() -
                        1)));
    o.set("getDate",
          fn((t, a)
                 -> num(toZoned(self(t).epochMillis, ZoneId.systemDefault())
                            .getDayOfMonth())));
    o.set("getDay",
          fn((t, a)
                 -> num(toZoned(self(t).epochMillis, ZoneId.systemDefault())
                            .getDayOfWeek()
                            .getValue() %
                        7)));
    o.set("getHours",
          fn((t, a)
                 -> num(toZoned(self(t).epochMillis, ZoneId.systemDefault())
                            .getHour())));
    o.set("getMinutes",
          fn((t, a)
                 -> num(toZoned(self(t).epochMillis, ZoneId.systemDefault())
                            .getMinute())));
    o.set("getSeconds",
          fn((t, a)
                 -> num(toZoned(self(t).epochMillis, ZoneId.systemDefault())
                            .getSecond())));
    o.set("getMilliseconds",
          fn((t, a)
                 -> num(toZoned(self(t).epochMillis, ZoneId.systemDefault())
                            .getNano() /
                        1_000_000)));

    o.set(
        "getUTCFullYear",
        fn((t,
            a) -> num(toZoned(self(t).epochMillis, ZoneOffset.UTC).getYear())));
    o.set("getUTCMonth",
          fn((t, a)
                 -> num(toZoned(self(t).epochMillis, ZoneOffset.UTC)
                            .getMonthValue() -
                        1)));
    o.set("getUTCDate",
          fn((t, a)
                 -> num(toZoned(self(t).epochMillis, ZoneOffset.UTC)
                            .getDayOfMonth())));
    o.set("getUTCDay",
          fn((t, a)
                 -> num(toZoned(self(t).epochMillis, ZoneOffset.UTC)
                            .getDayOfWeek()
                            .getValue() %
                        7)));
    o.set(
        "getUTCHours",
        fn((t,
            a) -> num(toZoned(self(t).epochMillis, ZoneOffset.UTC).getHour())));
    o.set(
        "getUTCMinutes",
        fn((t, a)
               -> num(
                   toZoned(self(t).epochMillis, ZoneOffset.UTC).getMinute())));
    o.set(
        "getUTCSeconds",
        fn((t, a)
               -> num(
                   toZoned(self(t).epochMillis, ZoneOffset.UTC).getSecond())));
    o.set("getUTCMilliseconds",
          fn((t, a)
                 -> num(toZoned(self(t).epochMillis, ZoneOffset.UTC).getNano() /
                        1_000_000)));

    o.set("getYear",
          fn((t, a)
                 -> num(toZoned(self(t).epochMillis, ZoneId.systemDefault())
                            .getYear() -
                        1900)));
    o.set("getTimezoneOffset", fn((t, a) -> {
            int secs = toZoned(self(t).epochMillis, ZoneId.systemDefault())
                           .getOffset()
                           .getTotalSeconds();
            return num(-secs / 60);
          }));

    o.set(
        "setFullYear",
        fn((t, a) -> setFromComponents(self(t), ZoneId.systemDefault(), a, 0)));
    o.set(
        "setMonth",
        fn((t, a) -> setFromComponents(self(t), ZoneId.systemDefault(), a, 1)));
    o.set(
        "setDate",
        fn((t, a) -> setFromComponents(self(t), ZoneId.systemDefault(), a, 2)));
    o.set(
        "setHours",
        fn((t, a) -> setFromComponents(self(t), ZoneId.systemDefault(), a, 3)));
    o.set(
        "setMinutes",
        fn((t, a) -> setFromComponents(self(t), ZoneId.systemDefault(), a, 4)));
    o.set(
        "setSeconds",
        fn((t, a) -> setFromComponents(self(t), ZoneId.systemDefault(), a, 5)));
    o.set(
        "setMilliseconds",
        fn((t, a) -> setFromComponents(self(t), ZoneId.systemDefault(), a, 6)));

    o.set("setUTCFullYear",
          fn((t, a) -> setFromComponents(self(t), ZoneOffset.UTC, a, 0)));
    o.set("setUTCMonth",
          fn((t, a) -> setFromComponents(self(t), ZoneOffset.UTC, a, 1)));
    o.set("setUTCDate",
          fn((t, a) -> setFromComponents(self(t), ZoneOffset.UTC, a, 2)));
    o.set("setUTCHours",
          fn((t, a) -> setFromComponents(self(t), ZoneOffset.UTC, a, 3)));
    o.set("setUTCMinutes",
          fn((t, a) -> setFromComponents(self(t), ZoneOffset.UTC, a, 4)));
    o.set("setUTCSeconds",
          fn((t, a) -> setFromComponents(self(t), ZoneOffset.UTC, a, 5)));
    o.set("setUTCMilliseconds",
          fn((t, a) -> setFromComponents(self(t), ZoneOffset.UTC, a, 6)));

    o.set("toISOString", fn((t, a) -> {
            if (Double.isNaN(self(t).epochMillis))
              throw new V6Throw(str("RangeError: Invalid time value"));
            return str(
                toZoned(self(t).epochMillis, ZoneOffset.UTC).format(ISO_FMT));
          }));
    o.set("toJSON",
          fn((t, a)
                 -> Double.isNaN(self(t).epochMillis)
                        ? new V6Value(V6Value.TAG_NULL, 0, null)
                        : str(toZoned(self(t).epochMillis, ZoneOffset.UTC)
                                  .format(ISO_FMT))));
    o.set("toString", fn((t, a)
                             -> Double.isNaN(self(t).epochMillis)
                                    ? str("Invalid Date")
                                    : str(toZoned(self(t).epochMillis,
                                                  ZoneId.systemDefault())
                                              .format(TO_STRING_FMT))));
    o.set("toDateString",
          fn((t, a)
                 -> str(toZoned(self(t).epochMillis, ZoneId.systemDefault())
                            .format(DATE_STRING_FMT))));
    o.set("toTimeString",
          fn((t, a)
                 -> str(toZoned(self(t).epochMillis, ZoneId.systemDefault())
                            .format(TIME_STRING_FMT))));
    o.set("toUTCString",
          fn((t, a)
                 -> str(toZoned(self(t).epochMillis, ZoneOffset.UTC)
                            .format(UTC_STRING_FMT))));
    o.set("toGMTString", o.get("toUTCString"));
    o.set("toLocaleDateString", o.get("toDateString"));
    o.set("toLocaleTimeString", o.get("toTimeString"));
    o.set("toLocaleString",
          fn((t, a)
                 -> str(toZoned(self(t).epochMillis, ZoneId.systemDefault())
                            .format(TO_STRING_FMT))));

    return o;
  }
}
