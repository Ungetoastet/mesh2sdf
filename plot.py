import json

import matplotlib.pyplot as plt
import numpy as np

plotsize = (10, 6)

plt.style.use('tableau-colorblind10')

with open("./output/benchmark_results.json", "r") as file:
    content = file.read()
    content = content.replace("nan,", "0,")
    data = json.loads(content)

def is_algorithm(name: str) -> bool:
    if name == "vertices" or name == "faces":
        return False
    return True

def get_sysinfo() -> str:
    s = data["system information"]
    return f"{s['cpu name']} - {s['gpu name']} - {s['ram size']} memory"

def plot_verts_to_wall(sdfSize:int):
    x = []
    wall_times = {}
    for algo in data["benchmark results"][list(data["benchmark results"].keys())[0]][f"sdf {sdfSize}"]:
        if not is_algorithm(algo):
            continue
        wall_times[algo] = []

    for model in data["benchmark results"].values():
        x.append(model["faces"])
        for algo in wall_times.keys():
            wall_times[algo].append(model[f"sdf {sdfSize}"][algo]["wall time"])

    fig, ax = plt.subplots(figsize=plotsize)

    for algo in wall_times.keys():
        ax.scatter(x, wall_times[algo], label=algo, s=0.2)

    ax.legend(markerscale=10)
    ax.set_xlabel("Anzahl Dreiecke")
    ax.set_ylabel("Ausführungszeit [s]")
    ax.set_yscale("log")
    ax.set_xscale("log")
    ax.set_title(f"Ausführungszeit - sdf Größe n={sdfSize}")
    fig.text(0.5, 0.005, get_sysinfo(), ha="center", fontsize=8, color="grey")
    plt.savefig(f"./paper/plots/verts_to_wall_{sdfSize}.png")

def plot_accuracies(sdfSize:int) -> None:
    values: dict[str, list[tuple[float, float]]] = {}
    extremes: dict[str, list[float]] = {}

    for algo in data["benchmark results"][list(data["benchmark results"].keys())[0]][f"sdf {sdfSize}"]:
        if not is_algorithm(algo):
            continue
        values[algo] = []
        extremes[algo] = [1.0, 0.0, 1.0, 0.0]  # samin, samax, famin, famax

    for model in data["benchmark results"].values():
        for algo in values.keys():
            sa: float = model[f"sdf {sdfSize}"][algo]["sign accuracy"]
            fa: float = model[f"sdf {sdfSize}"][algo]["face accuracy"]
            values[algo].append((sa, fa))
            extremes[algo][0] = min(sa, extremes[algo][0])
            extremes[algo][1] = max(sa, extremes[algo][1])
            extremes[algo][2] = min(fa, extremes[algo][2])
            extremes[algo][3] = max(fa, extremes[algo][3])

    sign_accs: list[float] = []
    face_accs: list[float] = []
    sign_stds: list[float] = []
    face_stds: list[float] = []

    for algo in values.keys():
        sign_vals = np.array([v[0] for v in values[algo]], dtype=float)
        face_vals = np.array([v[1] for v in values[algo]], dtype=float)

        sign_accs.append(float(np.mean(sign_vals)))
        face_accs.append(float(np.mean(face_vals)))

        sign_stds.append(float(np.std(sign_vals, ddof=1)))
        face_stds.append(float(np.std(face_vals, ddof=1)))
    
    barWidth: float = 0.2
    br1 = np.arange(len(sign_accs))
    br2 = br1 + barWidth

    fig, ax = plt.subplots(figsize=plotsize)

    errstyle = {
            "elinewidth": 1,      # thickness of error bar line
            "ecolor": "grey",    # color of error bars
            "capthick": 1         # thickness of caps
    }

    ax.bar(
        br1,
        sign_accs,
        width=barWidth,
        label="sign accuracy",
        yerr=sign_stds,
        capsize=6,
        error_kw=errstyle
    )

    ax.bar(
        br2,
        face_accs,
        width=barWidth,
        label="face accuracy",
        yerr=face_stds,
        capsize=6,
        error_kw=errstyle
    )
    
    ax.axhline(y=1, color="grey")
    fig.legend()

    ax.set_xticks(
        br1 + (0.5 * barWidth),
        list(values.keys()),
    )
    ax.set_title(f"Durchschnittliche Sign und Face accuracy - sdf Größe n={sdfSize}")
    ax.set_ylim((0,1))
    plt.savefig(f"./paper/plots/accs_{sdfSize}.png")

def plot_errors(sdfSize:int) -> None:
    values: dict[str, list[tuple[float, float]]] = {}
    extremes: dict[str, list[float]] = {}

    for algo in data["benchmark results"][list(data["benchmark results"].keys())[0]][f"sdf {sdfSize}"]:
        if not is_algorithm(algo):
            continue
        values[algo] = []
        extremes[algo] = [np.inf, 0.0, np.inf, 0.0]  # rmsemin, rmsemax, memin, memax

    for model in data["benchmark results"].values():
        for algo in values.keys():
            rmse: float = model[f"sdf {sdfSize}"][algo]["root mean squared error"]
            me: float = model[f"sdf {sdfSize}"][algo]["maximum absolute error"]
            values[algo].append((rmse, me))
            extremes[algo][0] = min(rmse, extremes[algo][0])
            extremes[algo][1] = max(rmse, extremes[algo][1])
            extremes[algo][2] = min(me, extremes[algo][2])
            extremes[algo][3] = max(me, extremes[algo][3])

    rmse_means: list[float] = []
    me_means: list[float] = []
    rmse_stds: list[float] = []
    me_stds: list[float] = []

    for algo in values.keys():
        rmse_vals = np.array([v[0] for v in values[algo]], dtype=float)
        me_vals = np.array([v[1] for v in values[algo]], dtype=float)

        rmse_means.append(float(np.mean(rmse_vals)))
        me_means.append(float(np.mean(me_vals)))

        rmse_stds.append(float(np.std(rmse_vals, ddof=1)))
        me_stds.append(float(np.std(me_vals, ddof=1)))

    barWidth: float = 0.2
    br1 = np.arange(len(rmse_means))
    br2 = br1 + barWidth

    fig, ax1 = plt.subplots(figsize=plotsize)
    ax2 = ax1.twinx()

    errstyle = {
        "elinewidth": 1,
        "ecolor": "grey",
        "capthick": 1
    }

    bars1 = ax1.bar(
        br1,
        rmse_means,
        width=barWidth,
        label="root mean squared error",
        # yerr=rmse_stds,
        capsize=6,
        error_kw=errstyle
    )

    bars2 = ax2.bar(
        br2,
        me_means,
        width=barWidth,
        label="maximum absolute error",
        # yerr=me_stds,
        capsize=6,
        error_kw=errstyle,
        color="red"
    )

    ax1.set_ylabel("RMSE")
    ax2.set_ylabel("Maximum Absolute Error")

    ax1.set_xticks(br1 + (0.5 * barWidth), list(values.keys()))
    ax1.set_title(f"Durchschnittliche Wertefehler - sdf Größe n={sdfSize}")

    fig.legend([bars1, bars2], ["root mean squared error", "maximum absolute error"])

    plt.savefig(f"./paper/plots/errs_{sdfSize}.png")

def plot_faces_to_sign_error(sdfSize: int) -> None:
    faces: list[int] = []
    sign_accs: dict[str, list[float]] = {}

    first = data["benchmark results"][list(data["benchmark results"].keys())[0]][f"sdf {sdfSize}"]
    for algo in first:
        if not is_algorithm(algo):
            continue
        sign_accs[algo] = []

    for model in data["benchmark results"].values():
        faces.append(model["faces"])
        for algo in sign_accs.keys():
            sa: float = model[f"sdf {sdfSize}"][algo]["sign accuracy"]
            sign_accs[algo].append(sa)

    for algo in sign_accs.keys():
        fig, ax = plt.subplots(figsize=plotsize)

        ax.scatter(faces, sign_accs[algo], s=2)

        ax.set_xlabel("face count")
        ax.set_ylabel("sign accuracy")
        ax.set_xscale("log")
        ax.set_ylim((0, 1.1))
        ax.set_title(f"Sign Accuracy vs Anzahl Dreiecke - {algo} - sdf Größe n={sdfSize}")

        plt.savefig(f"./paper/plots/faces_to_sign_error_{algo}_{sdfSize}.png")
        plt.close(fig)

if __name__ == "__main__":
    for sdfSize in [32, 64, 128, 256]:
        plot_verts_to_wall(sdfSize)
        plot_accuracies(sdfSize)
        plot_errors(sdfSize)
        plot_faces_to_sign_error(sdfSize)
    exit("plots completed")
