"""Draw the latest recorded Pico results: python3 benchmarks/plot_results.py"""
from pathlib import Path
import json
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

root = Path(__file__).resolve().parent
data = json.loads((root / "latest_results.json").read_text())
samples = np.array(data["samples"])
budget = samples / data["sample_rate_hz"] * 1000
methods = [
    ("Direct", "direct_ms", "#64748b"),
    ("F32 512", "fft_512_ms", "#ba6540"),
    ("F32 1024 · chosen", "fft_1024_ms", "#087f8c"),
    ("F32 2048", "fft_2048_ms", "#9673c0"),
]
plt.rcParams.update({"font.family": "DejaVu Sans", "font.size": 11})
fig, axes = plt.subplots(2, 1, figsize=(15, 10))
fig.subplots_adjust(left=.075, right=.98, top=.84, bottom=.13, hspace=.45)
for ax, selected, ceiling, title in [
    (axes[0], methods, 940, "All methods — direct and FFT 512 are TOO SLOW"),
    (axes[1], methods[2:], 110, "Closer look — FFT 1024 and 2048 are FAST ENOUGH at every size"),
]:
    ax.axhspan(0, 100, color="#e4f4ed", zorder=0)
    ax.axhline(100, color="#147d50", linestyle="--", linewidth=1.5)
    width = .8 / len(selected)
    for i, (name, key, color) in enumerate(selected):
        values = np.array(data[key]) / budget * 100
        x = np.arange(4) + (i - (len(selected)-1)/2)*width
        ax.bar(x, values, width=width*.9, color=color, label=name, zorder=3)
        for xx, value, ms in zip(x, values, data[key]):
            label = f"{ms:.3f} ms"
            if ax is axes[1]: label += "\nFAST ENOUGH"
            elif key in ("fft_1024_ms", "fft_2048_ms"): continue
            ax.text(xx, value + ceiling*.014, label, ha="center", va="bottom", fontsize=9)
    ax.set_ylim(0, ceiling)
    ax.set_xticks(np.arange(4))
    ax.set_xticklabels([f"{n:,} samples\n{b:.0f} ms available" for n,b in zip(samples,budget)])
    ax.set_ylabel("Available time used (%)")
    ax.set_title(title, loc="left", pad=12, fontsize=13, weight="bold")
    ax.grid(axis="y", color="#e2e8f0", zorder=0)
    ax.set_axisbelow(True)
    for side in ["top", "right"]: ax.spines[side].set_visible(False)
handles, labels = axes[0].get_legend_handles_labels()
fig.legend(handles, labels, loc="lower left", bbox_to_anchor=(.07,.875), ncol=4, frameon=False)
axes[1].text(3.48,102,"100% = processing deadline",ha="right",color="#147d50",fontsize=10)
free200 = min(1 - np.array(data["fft_1024_ms"]) / budget)*100
free300 = min(1 - np.array(data["fft_1024_ms"]) / (samples/300000*1000))*100
fig.text(.075,.97,"Latest Pico results · 30–50 kHz chirp",fontsize=24,weight="bold",color="#172b3a")
fig.text(.075,.933,f"F32 1024: at least {free200:.1f}% spare at 200 kHz; estimated {free300 + 1e-9:.1f}% at 300 kHz.",fontsize=13,color="#126340")
fig.text(.075,.055,"200 kHz sampling · 500-sample reference · all echo checks PASS at sample 300 · lower is better",fontsize=10,color="#475569")
fig.text(.075,.027,"Processing only. One-time setup: 4.797 ms. Live ADC/DMA capture and higher rates are not tested.",fontsize=10,color="#475569")
out=root / "images"
out.mkdir(exist_ok=True)
fig.savefig(out/"benchmark.png",dpi=180)
fig.savefig(out/"benchmark.svg")
