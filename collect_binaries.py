import os
import shutil

# Mapping of source project directory name to the target build output filename (base name)
PROJECT_MAPPING = {
    "tron_d2_test": "tron_d2_test",
    "tron_edge_fomo_cpu_type": "tron_edge_fomo_cpu_type",
    "tron_edge_fomo_ic": "tron_edge_fomo",
    "tron_edge_fomo_npu_type": "tron_edge_fomo_npu_type",
    "tron_i2c_test": "tron_i2c_test",
    "tron_img_cpu": "tron_mipi_test",
    "tron_img_npu": "tron_mipi_test",
    "tron_mipi_test_ori": "tron_mipi_test",
    "tron_serial_test": "tron_serial_test",
    "tron_yolo_face_cpu": "tron_mipi_test",
    "tron_yolo_face_npu": "tron_mipi_test"
}

def main():
    # Root path containing the script
    root_dir = os.path.dirname(os.path.abspath(__file__))
    src_dir = os.path.join(root_dir, "src")
    debug_dir = os.path.join(root_dir, "debug")
    
    # Create debug directory if it doesn't exist
    if not os.path.exists(debug_dir):
        os.makedirs(debug_dir)
        print(f"Created directory: {debug_dir}")
        
    copied_count = 0
    missing_count = 0
    
    print("Collecting binary outputs from projects...")
    for proj_dir, target_name in PROJECT_MAPPING.items():
        proj_path = os.path.join(src_dir, proj_dir)
        debug_output_dir = os.path.join(proj_path, "Debug")
        
        if not os.path.exists(debug_output_dir):
            print(f"[{proj_dir}] Debug directory not found, skipping...")
            missing_count += 1
            continue
            
        extensions = [".elf", ".srec"]
        found_any = False
        
        for ext in extensions:
            src_file_name = f"{target_name}{ext}"
            src_file_path = os.path.join(debug_output_dir, src_file_name)
            
            if os.path.exists(src_file_path):
                # Rename the output to match the project directory name for clarity
                dst_file_name = f"{proj_dir}{ext}"
                dst_file_path = os.path.join(debug_dir, dst_file_name)
                
                shutil.copy2(src_file_path, dst_file_path)
                print(f"  [{proj_dir}] Copied {src_file_name} -> debug/{dst_file_name}")
                copied_count += 1
                found_any = True
            else:
                # Check for alternative naming in case of clean build without secondary formats
                print(f"  [{proj_dir}] Source file not found: {src_file_name}")
                
        if not found_any:
            missing_count += 1
            
    print("\nSummary:")
    print(f"Successfully copied/updated {copied_count} files.")
    if missing_count > 0:
        print(f"Warnings: {missing_count} projects had missing or unbuilt outputs.")
    else:
        print("All project binaries collected successfully!")

if __name__ == "__main__":
    main()
