# --- <!-- custom: xml duplicates scanner (i would like to express/describe more what it does but cant be too bold about what it does as i don't understand it much for example whether it's a blind or deep scan and how they differ --> ---
# This script is part of the AdvCiv-SAS mod project.
# AI, UI, logging, or other modifications first developed in AdvCiv-SAS (Simple Advanced Strategy)
# (c) 2025 wonderingabout & AI/LLM helpers (see Authors in AdvCiv-SAS's root README.md)
#
# -*- coding: utf-8 -*-
import os
import xml.etree.ElementTree as ET
from datetime import datetime

# --- Configuration ---
root_folder = os.path.join("Assets", "XML")
# <!-- custom: share a common logs folder with other scripts (ideally have a subfolder per script filename) -->
#log_folder = "Logs_XML_Scans"
log_folder = os.path.join("Logs", "XML_Duplication_Scans")
os.makedirs(log_folder, exist_ok=True)

timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")

text_art = (
	"#\n"
	"# Auto-generated XML deep duplication scan report.\n"
	"# Created: " + timestamp + "\n"
	"#\n"
)

log_path = os.path.join(log_folder, f"scan_xml_duplicates_log_{timestamp}.txt")

warnings = []
total_duplicates = 0

# --- v3.3 UPDATE: Whitelist for structural "trivial" duplicates ---
TRIVIAL_DUPLICATES = {
	"iYield": {"0"},
	"iYieldChange": {"0"},
	"BonusType": {"NONE"},
	"PrereqTech": {"NONE"},
	"iRequired": {"1"},  # inside UnitMeshGroup
	"iProductionToCommerceModifier": {"0"},  # v3.3 ADD
	"iCommerce": {"0"},  # v3.3 ADD
	"bFlexible": {"0"},  # v3.3 ADD
}

def is_trivial_duplicate(field, value):
	allowed_values = TRIVIAL_DUPLICATES.get(field)
	return allowed_values is not None and value in allowed_values

# --- Helper Functions ---

def get_identity(child):
	# First subelement text if any, else self text
	subchildren = list(child)
	if subchildren:
		first_child = subchildren[0]
		return (first_child.tag.split("}", 1)[-1], (first_child.text or "").strip())
	else:
		return (None, (child.text or "").strip())

def scan_element(element, path, file_name, tag_positions):
	global total_duplicates

	# --- Group direct children by tag ---
	tag_groups = {}

	for child in element:
		tag = child.tag.split("}", 1)[-1]
		tag_groups.setdefault(tag, []).append(child)

	for tag, siblings in tag_groups.items():
		identities_seen = {}

		for child in siblings:
			identity_field, identity_value = get_identity(child)
			line = tag_positions.get(id(child), -1)

			key = (identity_field, identity_value)

			if key in identities_seen:
				# --- v3.3 UPDATE: Skip if trivial ---
				if is_trivial_duplicate(identity_field or tag, identity_value):
					continue

				message = (
					f"[DUPLICATE] File: {file_name} | Path: {path} | "
					f"Tag: <{tag}> | Line: {line} | "
					f"Field: {identity_field or 'TEXT'} | Value: '{identity_value}'"
				)
				print(message)
				warnings.append(message)
				total_duplicates += 1
			else:
				identities_seen[key] = True

	# Recurse
	for child in element:
		child_tag = child.tag.split("}", 1)[-1]
		scan_element(child, f"{path}/{child_tag}", file_name, tag_positions)

def scan_file(file_path):
	try:
		try:
			with open(file_path, "r", encoding="utf-8-sig") as f:
				file_text = f.read()
		except UnicodeDecodeError:
			with open(file_path, "r", encoding="latin1") as f:
				file_text = f.read()

		tree = ET.ElementTree(ET.fromstring(file_text))
		root = tree.getroot()
		file_lines = file_text.splitlines()

		tag_positions = find_tag_positions(file_lines, root)

		file_name = os.path.relpath(file_path, root_folder)
		scan_element(root, root.tag.split('}', 1)[-1], file_name, tag_positions)

		warnings.append("\n")
	except ET.ParseError as e:
		message = f"[ERROR] Could not parse {file_path}: {str(e)}"
		print(message)
		warnings.append(message)
		warnings.append("\n")

def find_tag_positions(file_lines, root):
	tag_positions = {}
	stack = [(root, 0)]

	while stack:
		element, start_line = stack.pop()
		tag = element.tag.split("}", 1)[-1]
		tag_open = f"<{tag}"

		for idx in range(start_line, len(file_lines)):
			if tag_open in file_lines[idx]:
				tag_positions[id(element)] = idx + 1
				break

		for child in reversed(list(element)):
			stack.append((child, idx))

	return tag_positions

# --- Main Scan Loop ---

print("Starting XML deep duplication scan...")

for dirpath, _, filenames in os.walk(root_folder):
	for filename in filenames:
		if filename.lower().endswith(".xml"):
			if filename.endswith("Schema.xml"):
				print(f"[SKIP] Skipping schema file: {filename}")
				continue
			full_path = os.path.join(dirpath, filename)
			scan_file(full_path)

print("Scan complete.")
print(f"Total duplicates found: {total_duplicates}")

# --- Log Results ---
if warnings:
	with open(log_path, "w", encoding="utf-8") as f:
		f.write(text_art.format(timestamp=timestamp))
		f.write("\n\n")
		for warning in warnings:
			f.write(warning + "\n")
		f.write("\n")
		f.write(f"Total duplicates found: {total_duplicates}\n")
	print(f"[OUTPUT] Warnings saved to: {log_path}")
else:
	print("[OK] No duplicate tags found!")
