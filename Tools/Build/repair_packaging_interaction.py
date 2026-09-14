"""Repair the audited Test_Main cast/function references with a native editor command.

The command backs up the map before changing it and saves only after Blueprint
compilation succeeds. See Saved/PackagingRepair for the original asset.
"""
import unreal

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
unreal.SystemLibrary.execute_console_command(world, 'FF.Packaging.RepairInteractionGraph apply')
