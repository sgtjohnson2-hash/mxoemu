import os
import glob
import xml.etree.ElementTree as ET
import random

MISSIONS_DIR = r"E:\Games\The Matrix Online\mxoemu_fork\Reality\Data\hd_dump\missions"

filler_objectives = [
    {
        "command": "DEFEAT",
        "idNpc": "9999",
        "description": "Fight through the ambush",
        "dial": "They knew we were coming! Defend yourself!",
        "spawnAmbush": "true"
    },
    {
        "command": "DEFEAT",
        "idNpc": "9998",
        "description": "Eliminate the guards blocking the path",
        "dial": "Intruder detected in sector 4."
    },
    {
        "command": "GIVE",
        "idNpc": "9997",
        "description": "Hack the terminal to open the gate",
        "dial": "Access granted.",
        "item": "1001",
        "isTimed": "true",
        "timeLimitSeconds": "300"
    },
    {
        "command": "DEFEAT",
        "idNpc": "9996",
        "description": "Survive the Exile hit squad",
        "dial": "The Frenchman sends his regards.",
        "spawnAmbush": "true"
    }
]

def process_file(filepath):
    tree = ET.parse(filepath)
    root = tree.getroot()
    data = root.find("data")
    
    if data is None:
        return

    # Extract existing objectives
    existing_objs = []
    for child in list(data):
        if child.tag.startswith("objective"):
            existing_objs.append(child)
            data.remove(child)

    new_objs = []
    for obj in existing_objs:
        # 50% chance to insert a filler objective before a TALK or DEFEAT objective
        if random.random() > 0.5:
            filler = random.choice(filler_objectives)
            filler_elem = ET.Element("objective_temp")
            for k, v in filler.items():
                filler_elem.set(k, v)
            new_objs.append(filler_elem)
        
        # Always append the original objective
        new_objs.append(obj)

    # Renumber and re-attach objectives
    for i, obj in enumerate(new_objs):
        obj.tag = f"objective{i+1}"
        data.append(obj)

    # Re-write the XML
    ET.indent(tree, space="  ", level=0)
    tree.write(filepath, encoding="utf-8", xml_declaration=True)
    print(f"Processed: {os.path.basename(filepath)} - Expanded to {len(new_objs)} objectives.")

def main():
    xml_files = glob.glob(os.path.join(MISSIONS_DIR, "*.xml"))
    for file in xml_files:
        process_file(file)
    print("Done expanding missions.")

if __name__ == "__main__":
    main()
