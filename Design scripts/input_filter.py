from ltiarithmetic import Z_R, Z_L, Z_C, s
import scipy.signal
import numpy
import texttable

import matplotlib.pyplot as plt

def system_plot(system, Z_in, Z_out):
    freq = numpy.logspace(1,6,1000)
    fig, axes = plt.subplots(1, 3, sharex=True, constrained_layout=True)
    two_pi = 2*numpy.pi

    layouts = (
        (system, axes[0], "Transfer function", "gain (dB)"),
        (Z_in, axes[1], "Input impedance", "impedance (dBΩ)"),
        (Z_out, axes[2], "Output impedance", "impedance (dBΩ)"),
    )

    for (S, ax, title, ylabel) in layouts:
        w, mag, phase = S.bode(two_pi*freq)
        ax.set_title(title)
        ax.set_xlabel('frequency (Hz)')
        ax.set_ylabel(ylabel)
        ax.semilogx(w/two_pi, mag)
        ax.grid(True, which="both", axis="x", color="xkcd:light grey")
        phase_ax = ax.twinx()
        phase_ax.set_ylabel('phase (deg)')
        phase_ax.semilogx(w/two_pi, phase, linestyle='dashed')

    return fig

# Set switching frequency and desired attenuation at fs
fs = 100e3
att_dB = 40

# VLS6045EX choices in 1-10 uH range
# L (H), R_L (Ω)
inductor_choices = (
    (1e-6, 0.01),
    (2.2e-6, .02),
    (3.3e-6, .02),
    (4.7e-6, .03),
    (6.8e-6, .04),
    (10e-6, .05),
)

print("EMI filter parameters for {0} dB attenuation at {1} Hz"
      .format(att_dB, fs))

tt = texttable.Texttable(max_width=130)
tt.set_deco(texttable.Texttable.HEADER)
tt.set_precision(2)
tt.set_cols_dtype(['e', 'f', 'e', 'f', 'e', 'f'])
tt.set_cols_align(['r', 'r', 'r', 'r', 'r', 'r'])
tt.header(['L (H)', 'R_L (Ω)', 'C (F)', 'ESR_C (Ω)', 'C_d (F)', 'ESR_C_d (Ω)'])

for (L, R_L) in inductor_choices:
    C_a = 1/(L * (2*numpy.pi*fs/10)**2)
    C_b = 1/L * (10**(att_dB/40) / (2*numpy.pi * fs))**2
    C = max(C_a, C_b)
    C_d = 4 * C

    # Choose ESR for Q=1
    R_C = numpy.sqrt(L/C) - R_L
    R_d = numpy.sqrt(L/C_d) - R_L

    tt.add_row([L, R_L, C, R_C, C_d, R_d])

print(tt.draw())

L = Z_L(10e-6) + Z_R(50e-3) # VLS6045EX-100M
C = Z_C(22e-6) + Z_R(100e-3) + Z_R(500e-3) # T520A226M006ATE100 + 500m resistor
Cd = Z_C(100e-6) + Z_R(45e-3) + Z_R(200e-3) # T520A107M006ATE045 + 200m resistor

Z_1 = L
Z_2 = C
Z_out = Z_1 | Z_2
Z_in = Cd | L

system = Z_out / Z_1

system_plot(system, Z_in, Z_out)
plt.show()
