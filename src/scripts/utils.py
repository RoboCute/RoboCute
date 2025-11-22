import os


def is_empty_folder(dir_path):
    if os.path.exists(dir_path) and not os.path.isfile(dir_path):
        # Check if directory is empty
        if not os.listdir(dir_path):
            return True
        return False
    else:
        # Treat non-existent directory as empty (ready for cloning)
        return True
