import os
Import("env")

# This script automatically generates/updates compile_commands.json
# which is used by clangd for IntelliSense.

def generate_compile_commands(source, target, env):
    print("Updating compilation database (compile_commands.json)...")
    # Run the compiledb target using the full path to the pio executable
    pio_path = "/home/linoux/.platformio/penv/bin/pio"
    env.Execute(f"{pio_path} run -t compiledb")

# Hook into the build process
# We use 'always_build' to ensure it runs even if nothing changed in the code,
# as the user might have changed platformio.ini settings.
env.AddCustomTarget(
    "update_compiledb",
    None,
    generate_compile_commands,
    title="Update Compiledb",
    description="Update compilation database for clangd"
)

# Trigger this on every build
env.AddPreAction("buildprog", generate_compile_commands)
