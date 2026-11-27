#!/usr/bin/env python3
"""Prepara il progetto Raymob per Palla VS Palla 3D (eseguito da .github/workflows/build.yml).
Uso: python3 android/patch_android.py android-build
"""
import glob, os, shutil, sys

root = sys.argv[1] if len(sys.argv) > 1 else "android-build"
cpp_dir = os.path.join(root, "app/src/main/cpp")

# 1) sorgenti del gioco -> modulo nativo (elenco automatico: nuovi file in src/ vengono inclusi da soli)
names = sorted(os.listdir("src"))
cpps = [n for n in names if n.endswith(".cpp")]
cs = [n for n in names if n.endswith(".c")]
for n in names:
    if n.endswith((".cpp", ".hpp", ".c", ".h")):
        shutil.copy(os.path.join("src", n), cpp_dir)
if os.path.exists(os.path.join(cpp_dir, "main.c")):
    os.remove(os.path.join(cpp_dir, "main.c"))

# 2) CMake di Raymob
p = os.path.join(cpp_dir, "CMakeLists.txt")
txt = open(p).read()
allsrc = " ".join(cpps + cs)
if "main.cpp" not in txt:
    if "main.c" in txt:
        txt = txt.replace("main.c", allsrc)
    elif "*.c" in txt and "*.cpp" not in txt:
        txt = txt.replace("*.c", "*.c *.cpp")
txt = "set(CMAKE_CXX_STANDARD 17)\n" + txt + "\nset_source_files_properties(" + " ".join(cpps) + " PROPERTIES COMPILE_OPTIONS -std=c++17)\n"
open(p, "w").write(txt)

# 3) nome app e id
p = os.path.join(root, "gradle.properties")
out = []
for l in open(p).read().splitlines():
    if l.startswith("app.name="):
        out.append("app.name=Palla VS Palla 3D")
    elif l.startswith("app.application_id="):
        out.append("app.application_id=com.pallavspalla.game")
    else:
        out.append(l)
open(p, "w").write("\n".join(out) + "\n")

# 4) permesso INTERNET
p = os.path.join(root, "app/src/main/AndroidManifest.xml")
m = open(p).read()
if "android.permission.INTERNET" not in m:
    m = m.replace("<application", '    <uses-permission android:name="android.permission.INTERNET" />\n    <application', 1)
    open(p, "w").write(m)

# 5) NativeLoader.java: notch/fullscreen + tastiera nativa + HTTP
methods = open("android/native_methods.java.txt").read()
hook = "super.onCreate(savedInstanceState);"
cutout = ("\n        if (android.os.Build.VERSION.SDK_INT >= 28) { android.view.WindowManager.LayoutParams lp = getWindow().getAttributes(); "
          "lp.layoutInDisplayCutoutMode = android.view.WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES; getWindow().setAttributes(lp); }")
found = False
for p in glob.glob(os.path.join(root, "app/src/main/java/**/NativeLoader.java"), recursive=True):
    found = True
    txt = open(p).read()
    if hook in txt and "LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES" not in txt:
        txt = txt.replace(hook, hook + cutout, 1)
    idx = txt.rfind("}")
    if idx != -1 and "callHttp" not in txt:
        txt = txt[:idx] + methods + "\n}\n"
    open(p, "w").write(txt)
if not found:
    print("ATTENZIONE: NativeLoader.java non trovato: tastiera e HTTP non saranno disponibili", file=sys.stderr)
    sys.exit(1)

# 6) utility Kotlin
dst = os.path.join(root, "app/src/main/java/com/pallavspalla/game")
os.makedirs(dst, exist_ok=True)
shutil.copy("android/GameUtils.kt", dst)
print("Patch Android completata:", " ".join(cpps + cs))
