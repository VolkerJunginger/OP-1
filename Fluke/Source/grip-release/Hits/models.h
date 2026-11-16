/* Fixed-point percussion adaptations of Braids oscillator topologies. */
static inline int random_sample(struct voice *v)
{
    v->random = v->random * 1664525u + 1013904223u;
    return (int)(v->random >> 16) - 32768;
}

static inline int oscillator(struct voice *v, int i, unsigned increment)
{
    v->phase[i] += increment;
    return sine[v->phase[i] >> 24];
}

static inline int modal(struct voice *v, const unsigned *step, int color, int drum, int extra)
{
    int body = oscillator(v, 0, step[0]);
    int upper = oscillator(v, 1, step[1]) + oscillator(v, 2, step[2]) / 2;
    int env = (int)(v->envelope >> 12);
    env = clamp(env + (4096 - env) * extra / 1024, 0, 4096);
    upper = upper * env >> 12;
    return body + (upper * (8192 + (color >> 1)) >> 15)
         + (drum && v->age < 16 ? random_sample(v) * (16 - (int)v->age) >> 7 : 0);
}

static inline int pluck(struct voice *v, unsigned length, int damping, int color)
{
    unsigned i = v->string_index;
    if (v->string_fill < length) i = v->string_fill;
    if (i >= length) i = 0;
    unsigned next = i + 1 == length ? 0 : i + 1;
    int sample;
    if (v->string_fill < length) {
        int noise = random_sample(v);
        v->low += (noise - v->low) * (4096 + (color >> 1)) >> 15;
        sample = v->low;
        ++v->string_fill;
    } else {
        sample = ((v->string[i] + v->string[next]) >> 1) * damping >> 15;
    }
    v->string[i] = (short)sample;
    v->string_index = next;
    return sample;
}

static inline int filtered_noise(struct voice *v, int cutoff, int damp, int color)
{
    struct filter *f = v->filters;
    int high = svf(f, random_sample(v) >> 1, cutoff, damp, 1);
    int second = color < 16384 ? f->low : clamp(high, -32768, 32767);
    int blend = color < 16384 ? 16384 - color : color - 16384;
    return f->band + ((second - f->band) * blend >> 14);
}

static inline int resonant_noise(struct voice *v, int f1, int f2, int damp,
                                 int density, int particle)
{
    int in = random_sample(v);
    if (particle && v->age && (v->random & 0x7fffu) >= (unsigned)density)
        in = 0;
    return (svf(v->filters, in >> 1, f1, damp, 0)
          + svf(v->filters + 1, in >> 1, f2, damp, 0)) >> 1;
}

static inline int clocked(struct voice *v, unsigned step, int color, int qpsk)
{
    v->clock += step;
    if (v->clock < step || v->age == 0)
        v->low = random_sample(v);
    if (!qpsk) {
        unsigned mask = 0xffffu << (color >> 12);
        return (short)((unsigned)v->low & mask);
    }
    unsigned carrier = v->phase[0] += (step >> 3) * (16u + (unsigned)(color >> 10));
    return sine[((carrier >> 24) + ((unsigned)v->low >> 8 & 0xc0u)) & 255u];
}

static inline int cloud(struct voice *v, const unsigned *step, unsigned grain_rate)
{
    int sum = 0;
    for (int i = 0; i < 3; ++i) {
        unsigned position = v->phase[3+i] >> 24;
        int env = position < 128 ? (int)position : 255 - (int)position;
        v->phase[3+i] += grain_rate + (grain_rate >> (i + 1));
        sum += oscillator(v, i, step[i]) * env >> 8;
    }
    return sum;
}

static inline int fm_hit(struct voice *v, unsigned root, unsigned mod, int index, int feedback)
{
    int carrier_mod = oscillator(v, 1, mod);
    if (feedback)
        carrier_mod = sine[(v->phase[1] + (unsigned)(v->low * feedback)) >> 24];
    v->phase[0] += root;
    v->low = sine[(v->phase[0] + (unsigned)(carrier_mod * index)) >> 24];
    return v->low;
}

static inline int toy_hit(struct voice *v, unsigned step, int bits, int color)
{
    int wave = oscillator(v, 0, step);
    unsigned mask = 0xffffu << bits;
    return (short)((unsigned)(wave + ((wave > 0 ? color : -color) >> 2)) & mask);
}

static inline int ring_hit(struct voice *v, const unsigned *step, int color)
{
    int a = oscillator(v, 0, step[0]);
    int b = oscillator(v, 1, step[1]);
    int c = oscillator(v, 2, step[2]);
    return ((a * b >> 14) * c >> 14) + (a * color >> 16);
}
