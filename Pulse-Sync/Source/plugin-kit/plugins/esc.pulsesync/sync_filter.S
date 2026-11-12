/* GPL-3.0-or-later. Preserve the stock getter caller registers. */
.text
.align 2
.global _pulse_sync_filter
.type _pulse_sync_filter, STT_FUNC
_pulse_sync_filter:
    [--SP] = RETS;
    [--SP] = ASTAT;
    [--SP] = R1;
    [--SP] = R2;
    [--SP] = R3;
    [--SP] = P0;
    [--SP] = P1;
    [--SP] = P2;
    [--SP] = LC0;
    [--SP] = LT0;
    [--SP] = LB0;
    [--SP] = LC1;
    [--SP] = LT1;
    [--SP] = LB1;
    [--SP] = A0.X;
    [--SP] = A0.W;
    [--SP] = A1.X;
    [--SP] = A1.W;
    [--SP] = I0;
    [--SP] = I1;
    [--SP] = I2;
    [--SP] = I3;
    [--SP] = M0;
    [--SP] = M1;
    [--SP] = M2;
    [--SP] = M3;
    [--SP] = B0;
    [--SP] = B1;
    [--SP] = B2;
    [--SP] = B3;
    [--SP] = L0;
    [--SP] = L1;
    [--SP] = L2;
    [--SP] = L3;
    R0 = P5;
    L0 = 0;
    L1 = 0;
    L2 = 0;
    L3 = 0;
    SP += -12;
    CALL _clock_sync_mode;
    SP += 12;
    L3 = [SP++];
    L2 = [SP++];
    L1 = [SP++];
    L0 = [SP++];
    B3 = [SP++];
    B2 = [SP++];
    B1 = [SP++];
    B0 = [SP++];
    M3 = [SP++];
    M2 = [SP++];
    M1 = [SP++];
    M0 = [SP++];
    I3 = [SP++];
    I2 = [SP++];
    I1 = [SP++];
    I0 = [SP++];
    A1.W = [SP++];
    A1.X = [SP++];
    A0.W = [SP++];
    A0.X = [SP++];
    LB1 = [SP++];
    LT1 = [SP++];
    LC1 = [SP++];
    LB0 = [SP++];
    LT0 = [SP++];
    LC0 = [SP++];
    P2 = [SP++];
    P1 = [SP++];
    P0 = [SP++];
    R3 = [SP++];
    R2 = [SP++];
    R1 = [SP++];
    ASTAT = [SP++];
    RETS = [SP++];
    RTS;

/* R246 F8-only call site. The preceding sync query records the packet tag
 * before the parser repurposes P5. R1/R2 are the timestamp arguments.
 * Tail-jump with RETS intact so stock returns straight to the parser. */
.align 2
.global _pulse_clock_timestamp
.type _pulse_clock_timestamp, STT_FUNC
_pulse_clock_timestamp:
    [--SP] = ASTAT;
    [--SP] = R3;
    P0.L = _pulse_use_current_time;
    P0.H = _pulse_use_current_time;
    R3 = [P0];
    CC = R3 == 0;
    IF CC JUMP .Lstock_timestamp;
    R1 = 0;
    R2 = 0;
.Lstock_timestamp:
    R3 = [SP++];
    ASTAT = [SP++];
    P0.L = 0xeb30;
    P0.H = 0x01a5;
    JUMP (P0);
