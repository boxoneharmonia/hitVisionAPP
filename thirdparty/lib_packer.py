import os
import argparse

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--sodir")
    parser.add_argument("--gniname")

    args = parser.parse_args()

    sodir = args.sodir
    gniname = args.gniname
    varname = args.gniname 
    if not gniname.endswith(".gni"):
        gniname += ".gni"
    output_gni = os.path.join(sodir, gniname)

    if not os.path.isdir(sodir):
        print(f"Error: {sodir} is not a valid directory.")
        return

    lib_names = []
    for fname in os.listdir(sodir):
        if fname.startswith("lib") and fname.endswith(".so"):
            name = fname[len("lib"):-len(".so")]
            lib_names.append(name)

    if not lib_names:
        print(f"No .so files found in {sodir}.")
        return

    lib_names.sort()

    with open(output_gni, "w") as f:
        f.write(f"{varname} = [\n")
        for name in lib_names:
            f.write(f'  "{name}",\n')
        f.write("]\n")

    print(f"GNI file generated: {output_gni}")
    print(f"{len(lib_names)} libraries found.")

if __name__ == "__main__":
    main()
