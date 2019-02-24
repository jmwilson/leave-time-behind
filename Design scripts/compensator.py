import math
import numpy
import scipy
from ltiarithmetic import Z_R, Z_C, Z_L
import matplotlib.pyplot as plt

def transfer_plot(system):
    freq = numpy.logspace(1,6,30 * 6)
    fig, ax = plt.subplots(constrained_layout=True)
    two_pi = 2 * math.pi

    w, mag, phase = system.bode(two_pi * freq)
    ax.set_title("Loop gain")
    ax.set_xlabel("frequency (Hz)")
    ax.set_ylabel("gain (dB)")
    ax.semilogx(w/two_pi, mag)
    ax.grid(True, which="both", axis="x", color="xkcd:light grey")
    phase_ax = ax.twinx()
    phase_ax.set_ylabel("phase (deg)")
    phase_ax.semilogx(w/two_pi, phase, linestyle="dashed")

    return fig

# LM3478 error amplifier characteristics
G_m = 800e-6  # ohm^-1
R_o = 47.5e3 # ohm

# LM3478 current mode characteristics
V_sl = 92e-3 # V
I_sl = 40e-6 # A

# Design parameters
V = 150. # V
V_g = 5. # V
R_1 = 1.5e6 # Ohm
R_2 = 12.7e3 # Ohm
f_s = 100e3 # Hz
L = 15e-6 # H
R_f = 39e-3 # Ohm
C = 100e-9 # F
R_sl = 470. # Ohm
R = 10e3 # Ohm

# Desired crossover frequency and phase margin
f_c = f_s / 10
p_m = 60. # degrees

# Conversion ratio derived from design parameters
M = V / V_g

# Current mode slopes derived from characteristics and design parameters (A/s)
s_n = V_g / L * R_f
s_f = (V_g - V) / L * R_f
s_e = (V_sl + I_sl * R_sl) * f_s

# G_vc: control to output gain
f_2 = math.sqrt(2) / (R * (1 + s_e/s_n) * math.sqrt((M - 1)/M * 1/(L*R*f_s)))
r_2 = R * (M - 1)/M
G_vc = 1/R_f * f_2 * (Z_R(r_2) | Z_R(R) | Z_C(C))

# H: resistive divider sensor gain
H = R_2 / (R_1 + R_2)

# G_u: uncompensated error amplifier gain
G_u = G_m * Z_R(R_o)

# T_u: Uncompensated loop gain
T_u = H * G_u * G_vc

# Find uncompensated gain at desired crossover and get starting point for R_c
_, [T_u_f_c] = T_u.freqresp([2 * math.pi * f_c])
gain_f_c_u = numpy.abs(T_u_f_c)
R_c_0 = R_o / (gain_f_c_u - 1)

# Get starting point for C_c1
phase_f_c_u = numpy.angle(T_u_f_c, deg=True)
required_phase_adj = (p_m - 180.) - phase_f_c_u
f_z = f_c * math.exp(math.pi/2 * required_phase_adj/45)
C_c1_0 = 1/(2 * math.pi * f_z * R_c_0)

def loop_system(x, f, margin):
    r_c, c_c1 = x
    G_c = G_m * (Z_R(R_o) | (Z_R(r_c) + Z_C(c_c1)))
    system = H * G_c * G_vc
    _, [resp] = system.freqresp([2* math.pi * f])
    gain = numpy.abs(resp)
    phase_margin = 180. + numpy.angle(resp, deg=True)
    return (gain - 1., phase_margin - margin)

[R_c, C_c1] = scipy.optimize.fsolve(
    loop_system,
    [R_c_0, C_c1_0],
    (f_c, p_m)
)

print(
    "Compensation network for crossover at {:g} Hz with {:g}°"
    " phase margin: R_c = {:g}, C_c1 = {:g}".format(
    f_c, p_m, R_c, C_c1
))

R_c = 4.3e3
C_c1 = 5.6e-9

# G_c: compensated error amplifier gain
G_c = G_m * (Z_R(R_o) | (Z_R(R_c) + Z_C(C_c1)))

# T: compensated loop gain
T = H * G_c * G_vc

def system_crossover(f, system):
    _, [resp] = system.freqresp([2 * math.pi * f])
    return numpy.abs(resp) - 1

[crossover] = scipy.optimize.fsolve(system_crossover, f_c, T)
_, [T_0, T_crossover] = T.freqresp([0, 2 * math.pi * crossover])
gain_0 = numpy.abs(T_0)
phase_margin = numpy.angle(T_crossover, deg=True) + 180.
print("Chosen compensation network: R_c = {:.2g}, C_c1 = {:.2g}".format(R_c, C_c1))
print("Crossover at {:.2g} Hz, phase margin: {:.2g}°".format(crossover, phase_margin))
print("DC loop gain: {:.2g} dB".format(20 * math.log10(gain_0)))

transfer_plot(T)
plt.show()
