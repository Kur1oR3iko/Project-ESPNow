from PIL import Image
from pyzbar.pyzbar import decode as decode_qr
import qrcode

def decode_qr_from_image(image_path):
    """Decodes QR code from an image and returns the data."""
    try:
        img = Image.open(image_path)
        decoded_objects = decode_qr(img)
        if decoded_objects:
            return decoded_objects[0].data.decode('utf-8')
        else:
            print(f"No QR code found or could not be decoded in '{image_path}'.")
            return None
    except FileNotFoundError:
        print(f"Error: Image file not found at '{image_path}'")
        return None
    except Exception as e:
        print(f"An error occurred while decoding '{image_path}': {e}")
        return None

def text_to_char_qr(text_data, output_path):
    """Generates a QR code from text and converts it to 'E' and '.' representation."""
    if not text_data:
        return

    try:
        # Generate QR code object
        qr = qrcode.QRCode(
            version=1, # Keep it simple, auto-adjusts if needed
            error_correction=qrcode.constants.ERROR_CORRECT_L,
            box_size=1, # Each 'pixel' of the QR code will be 1x1
            border=0    # Smallest possible border for compactness
        )
        qr.add_data(text_data)
        qr.make(fit=True)

        # The qr.modules is a list of lists of booleans (True for black, False for white)
        qr_matrix = qr.modules
        
        text_qr_art = []
        for row in qr_matrix:
            row_str = ""
            for module in row:
                if module:  # True (black module)
                    row_str += "E"
                else:       # False (white module)
                    row_str += "."
            text_qr_art.append(row_str)

        with open(output_path, 'w') as f:
            for row_str in text_qr_art:
                f.write(row_str + '\n')
        print(f"Successfully generated text QR for data and saved to '{output_path}'")

    except Exception as e:
        print(f"An error occurred while generating text QR for data '{text_data[:30]}...': {e}")


if __name__ == "__main__":
    images_to_process = {
        "shapaper blog.png": "shapaper_blog.txt",
        "原作者空间.png": "原作者空间.txt",
    }

    # Check for dependencies
    try:
        from PIL import Image
        from pyzbar import pyzbar
        import qrcode
    except ImportError as e:
        print(f"Missing one or more required libraries: Pillow, pyzbar, qrcode. Error: {e}")
        print("Please install them by running:")
        print("pip install Pillow pyzbar qrcode")
        exit()

    for img_file, txt_file in images_to_process.items():
        print(f"\nProcessing '{img_file}'...")
        decoded_text = decode_qr_from_image(img_file)
        if decoded_text:
            print(f"Decoded data: {decoded_text}")
            text_to_char_qr(decoded_text, txt_file)
        else:
            print(f"Skipping text QR generation for '{img_file}' due to decoding failure.")
