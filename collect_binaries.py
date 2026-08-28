import os
import shutil

# Mapping of source project directory name to:
# (target_build_output_filename, subfolder_for_srec)
# If subfolder_for_srec is "", the .srec file is copied to the root of debug/
PROJECT_CONFIG = {
    "tron_yolo_face_npu": ("tron_mipi_test", ""),              # Main NPU (root)
    "tron_img_npu": ("tron_mipi_test", ""),                    # Main NPU (root)
    "tron_edge_fomo_npu_type": ("tron_edge_fomo_npu_type", ""),# Main NPU (root)
    
    "tron_yolo_face_cpu": ("tron_mipi_test", "cpu_ai_versions"),
    "tron_img_cpu": ("tron_mipi_test", "cpu_ai_versions"),
    "tron_edge_fomo_cpu_type": ("tron_edge_fomo_cpu_type", "cpu_ai_versions"),
    
    "tron_serial_test": ("tron_serial_test", "base_firmware"),
    "tron_i2c_test": ("tron_i2c_test", "base_firmware"),
    "tron_d2_test": ("tron_d2_test", "base_firmware"),
    "tron_mipi_test_ori": ("tron_mipi_test", "base_firmware"),
    "tron_edge_fomo_ic": ("tron_edge_fomo", "base_firmware")   # Reference
}

def clean_srec(src_path, dst_path):
    """
    Reads an S-Record file and writes to dst_path, omitting any lines that write to
    the Option-Setting Memory (OFS) registers (address prefix 0x02C9) to prevent
    address errors in Renesas Flash Programmer.
    """
    try:
        with open(src_path, "r") as src, open(dst_path, "w") as dst:
            skipped_lines = 0
            for line in src:
                # S3 records contain 4-byte addresses. Address starts at index 4 (length 8 hex chars).
                # Check if the address prefix is 02C9 (OFS memory area)
                if line.startswith("S3") and line[4:8].upper() == "02C9":
                    skipped_lines += 1
                    continue
                dst.write(line)
        if skipped_lines > 0:
            print(f"    -> Filtered {skipped_lines} lines of OFS registers (0x02C9xxxx) from SREC.")
    except Exception as e:
        print(f"    -> Error cleaning SREC: {e}")
        shutil.copy2(src_path, dst_path)

def main():
    root_dir = os.path.dirname(os.path.abspath(__file__))
    src_dir = os.path.join(root_dir, "src")
    debug_dir = os.path.join(root_dir, "debug")
    
    # Re-create clean debug directory to clear previous flat structure
    if os.path.exists(debug_dir):
        print(f"Clearing old debug directory: {debug_dir}")
        shutil.rmtree(debug_dir)
    
    os.makedirs(debug_dir)
    print(f"Created clean debug directory: {debug_dir}")
    
    # Pre-create subfolders
    subfolders = ["base_firmware", "cpu_ai_versions", "elf_files"]
    for sub in subfolders:
        os.makedirs(os.path.join(debug_dir, sub), exist_ok=True)
        
    copied_count = 0
    missing_count = 0
    
    print("\nCollecting binary outputs from projects...")
    for proj_dir, (target_name, srec_sub) in PROJECT_CONFIG.items():
        proj_path = os.path.join(src_dir, proj_dir)
        debug_output_dir = os.path.join(proj_path, "Debug")
        
        if not os.path.exists(debug_output_dir):
            print(f"[{proj_dir}] Debug directory not found, skipping...")
            missing_count += 1
            continue
            
        # 1. Process SREC file
        srec_name = f"{target_name}.srec"
        srec_path = os.path.join(debug_output_dir, srec_name)
        if os.path.exists(srec_path):
            dst_name = f"{proj_dir}.srec"
            if srec_sub:
                dst_path = os.path.join(debug_dir, srec_sub, dst_name)
                dest_label = f"debug/{srec_sub}/{dst_name}"
            else:
                dst_path = os.path.join(debug_dir, dst_name)
                dest_label = f"debug/{dst_name}"
                
            clean_srec(srec_path, dst_path)
            print(f"  [{proj_dir}] Copied SREC -> {dest_label}")
            copied_count += 1
        else:
            print(f"  [{proj_dir}] SREC file not found: {srec_name}")
            
        # 2. Process ELF file (Always move all ELF files to debug/elf_files/)
        elf_name = f"{target_name}.elf"
        elf_path = os.path.join(debug_output_dir, elf_name)
        if os.path.exists(elf_path):
            dst_name = f"{proj_dir}.elf"
            dst_path = os.path.join(debug_dir, "elf_files", dst_name)
            
            shutil.copy2(elf_path, dst_path)
            print(f"  [{proj_dir}] Copied ELF  -> debug/elf_files/{dst_name}")
            copied_count += 1
        else:
            print(f"  [{proj_dir}] ELF file not found: {elf_name}")
            
    print("\nSummary:")
    print(f"Successfully copied/updated {copied_count} files.")
    if missing_count > 0:
        print(f"Warnings: {missing_count} projects had missing or unbuilt outputs.")
    else:
        print("All project binaries collected and organized successfully!")

if __name__ == "__main__":
    main()
