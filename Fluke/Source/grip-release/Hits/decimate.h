static int setting_matches(const char *a, const char *b)
{
    while (*a && *a == *b) { ++a; ++b; }
    return *a == *b;
}

static void set_decimate(const char *name, int value)
{
    if (setting_matches(name, "decimate_scope")) {
        decimate_global = value == 1;
        return;
    }
    if (setting_matches(name, "decimate_depth")) {
        global_depth = (unsigned)clamp(value, 0, 3);
        return;
    }
}

static void drum_bus(int instrument, short *buf)
{
    if (instrument != 1)
        return;
    unsigned depth = decimate_global ? global_depth : 0;
    if (depth) {
        int step = 1 << (depth * 4);
        for (int i = 0; i < 64; ++i)
            buf[i] = (short)quantize(buf[i], step);
    }
}
