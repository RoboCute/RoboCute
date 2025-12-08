"""
Text2Image Nodes for RoboCute

Provides nodes for Stable Diffusion text-to-image generation, inspired by ComfyUI.
This is a simplified implementation that demonstrates the node system.
"""

from typing import Dict, Any, List, Optional
import base64
import io
import robocute as rbc

# Try to import torch and diffusers, but make it optional
try:
    import torch
    from diffusers import StableDiffusionPipeline, DPMSolverMultistepScheduler
    DIFFUSERS_AVAILABLE = True
except ImportError:
    DIFFUSERS_AVAILABLE = False
    print("[Text2Image] Warning: diffusers not available. Text2Image nodes will not work.")


@rbc.register_node
class CLIPTextEncodeNode(rbc.RBCNode):
    """
    CLIP Text Encode Node - Encode text prompt using CLIP
    
    Similar to ComfyUI's CLIPTextEncode node, this encodes text prompts
    into embeddings that can be used by the diffusion model.
    """

    NODE_TYPE = "clip_text_encode"
    DISPLAY_NAME = "CLIP Text Encode"
    CATEGORY = "Text2Image"
    DESCRIPTION = "Encode text prompt into CLIP embeddings for Stable Diffusion"

    @classmethod
    def get_inputs(cls) -> List[rbc.NodeInput]:
        return [
            rbc.NodeInput(
                name="text",
                type="string",
                required=True,
                default="",
                description="Text prompt to encode",
            ),
        ]

    @classmethod
    def get_outputs(cls) -> List[rbc.NodeOutput]:
        return [
            rbc.NodeOutput(
                name="conditioning",
                type="conditioning",
                description="CLIP text conditioning embeddings",
            )
        ]

    def execute(self) -> Dict[str, Any]:
        text = self.get_input("text", "")
        
        if not text:
            raise ValueError("Text input is required")
        
        print(f"[CLIPTextEncode] Encoding text: {text[:50]}...")
        
        # For a simplified implementation, we just return the text
        # In a full implementation, this would encode using CLIP
        # For now, we'll pass the text to the sampler which will handle encoding
        return {
            "conditioning": {
                "text": text,
                "type": "clip_conditioning"
            }
        }


@rbc.register_node
class EmptyLatentImageNode(rbc.RBCNode):
    """
    Empty Latent Image Node - Generate empty latent tensor
    
    Similar to ComfyUI's EmptyLatentImage node, this creates an empty
    latent tensor with specified dimensions.
    """

    NODE_TYPE = "empty_latent_image"
    DISPLAY_NAME = "Empty Latent Image"
    CATEGORY = "Text2Image"
    DESCRIPTION = "Generate empty latent tensor with specified dimensions"

    @classmethod
    def get_inputs(cls) -> List[rbc.NodeInput]:
        return [
            rbc.NodeInput(
                name="width",
                type="integer",
                required=False,
                default=512,
                description="Image width",
            ),
            rbc.NodeInput(
                name="height",
                type="integer",
                required=False,
                default=512,
                description="Image height",
            ),
            rbc.NodeInput(
                name="batch_size",
                type="integer",
                required=False,
                default=1,
                description="Batch size",
            ),
        ]

    @classmethod
    def get_outputs(cls) -> List[rbc.NodeOutput]:
        return [
            rbc.NodeOutput(
                name="latent",
                type="latent",
                description="Empty latent tensor",
            )
        ]

    def execute(self) -> Dict[str, Any]:
        width = int(self.get_input("width", 512))
        height = int(self.get_input("height", 512))
        batch_size = int(self.get_input("batch_size", 1))
        
        print(f"[EmptyLatentImage] Creating latent: {batch_size}x{height}x{width}")
        
        # Return metadata for latent creation
        # Actual tensor creation happens in the sampler
        return {
            "latent": {
                "width": width,
                "height": height,
                "batch_size": batch_size,
                "type": "empty_latent"
            }
        }


@rbc.register_node
class KSamplerNode(rbc.RBCNode):
    """
    KSampler Node - Sample from diffusion model
    
    Similar to ComfyUI's KSampler node, this performs the actual
    diffusion sampling process to generate images from text.
    """

    NODE_TYPE = "ksampler"
    DISPLAY_NAME = "KSampler"
    CATEGORY = "Text2Image"
    DESCRIPTION = "Sample image from diffusion model using text prompt"

    def __init__(self, node_id: str, context: Optional[Any] = None):
        super().__init__(node_id, context)
        self._pipeline: Optional[Any] = None

    @classmethod
    def get_inputs(cls) -> List[rbc.NodeInput]:
        return [
            rbc.NodeInput(
                name="conditioning",
                type="conditioning",
                required=True,
                description="CLIP text conditioning from CLIPTextEncode",
            ),
            rbc.NodeInput(
                name="latent",
                type="latent",
                required=True,
                description="Empty latent tensor from EmptyLatentImage",
            ),
            rbc.NodeInput(
                name="seed",
                type="integer",
                required=False,
                default=-1,
                description="Random seed (-1 for random)",
            ),
            rbc.NodeInput(
                name="steps",
                type="integer",
                required=False,
                default=20,
                description="Number of diffusion steps",
            ),
            rbc.NodeInput(
                name="cfg_scale",
                type="number",
                required=False,
                default=7.0,
                description="Classifier-free guidance scale",
            ),
            rbc.NodeInput(
                name="model_name",
                type="string",
                required=False,
                default="runwayml/stable-diffusion-v1-5",
                description="Stable Diffusion model name",
            ),
        ]

    @classmethod
    def get_outputs(cls) -> List[rbc.NodeOutput]:
        return [
            rbc.NodeOutput(
                name="latent",
                type="latent",
                description="Generated latent tensor",
            )
        ]

    def _get_pipeline(self, model_name: str):
        """Get or create the diffusion pipeline"""
        if not DIFFUSERS_AVAILABLE:
            raise RuntimeError(
                "diffusers library not available. "
                "Install it with: pip install diffusers transformers accelerate"
            )
        
        if self._pipeline is None or self._pipeline.model_name != model_name:
            print(f"[KSampler] Loading model: {model_name}")
            self._pipeline = StableDiffusionPipeline.from_pretrained(
                model_name,
                torch_dtype=torch.float16 if torch.cuda.is_available() else torch.float32,
            )
            
            if torch.cuda.is_available():
                self._pipeline = self._pipeline.to("cuda")
                print("[KSampler] Using CUDA")
            else:
                print("[KSampler] Using CPU")
            
            # Use DPM++ scheduler for better quality
            self._pipeline.scheduler = DPMSolverMultistepScheduler.from_config(
                self._pipeline.scheduler.config
            )
            self._pipeline.model_name = model_name  # Store for comparison
        
        return self._pipeline

    def execute(self) -> Dict[str, Any]:
        if not DIFFUSERS_AVAILABLE:
            raise RuntimeError(
                "diffusers library not available. "
                "Install it with: pip install diffusers transformers accelerate"
            )
        
        conditioning = self.get_input("conditioning")
        latent_info = self.get_input("latent")
        seed = int(self.get_input("seed", -1))
        steps = int(self.get_input("steps", 20))
        cfg_scale = float(self.get_input("cfg_scale", 7.0))
        model_name = self.get_input("model_name", "runwayml/stable-diffusion-v1-5")
        
        if conditioning is None:
            raise ValueError("Conditioning input is required")
        if latent_info is None:
            raise ValueError("Latent input is required")
        
        # Extract text from conditioning
        text = conditioning.get("text", "")
        if not text:
            raise ValueError("No text in conditioning")
        
        width = latent_info.get("width", 512)
        height = latent_info.get("height", 512)
        
        print(f"[KSampler] Sampling image: {width}x{height}")
        print(f"[KSampler] Prompt: {text[:50]}...")
        print(f"[KSampler] Steps: {steps}, CFG: {cfg_scale}, Seed: {seed}")
        
        # Get pipeline
        pipeline = self._get_pipeline(model_name)
        
        # Set seed
        if seed == -1:
            seed = torch.randint(0, 2**32, (1,)).item()
        generator = torch.Generator()
        if torch.cuda.is_available():
            generator = generator.manual_seed(seed)
        else:
            generator = generator.manual_seed(seed)
        
        # Generate image
        print("[KSampler] Generating image...")
        with torch.no_grad():
            image = pipeline(
                prompt=text,
                width=width,
                height=height,
                num_inference_steps=steps,
                guidance_scale=cfg_scale,
                generator=generator,
            ).images[0]
        
        print("[KSampler] ✓ Image generated")
        
        # Convert PIL image to base64 for storage/transmission
        buffer = io.BytesIO()
        image.save(buffer, format="PNG")
        image_bytes = buffer.getvalue()
        image_base64 = base64.b64encode(image_bytes).decode("utf-8")
        
        return {
            "latent": {
                "image": image_base64,
                "width": width,
                "height": height,
                "format": "PNG",
                "type": "generated_latent",
                "seed": seed,
            }
        }


@rbc.register_node
class VAEDecodeNode(rbc.RBCNode):
    """
    VAE Decode Node - Decode latent to image
    
    In a full implementation, this would decode the latent tensor using VAE.
    For this simplified version, we just pass through the image if it's already decoded.
    """

    NODE_TYPE = "vae_decode"
    DISPLAY_NAME = "VAE Decode"
    CATEGORY = "Text2Image"
    DESCRIPTION = "Decode latent tensor to image (pass-through in simplified version)"

    @classmethod
    def get_inputs(cls) -> List[rbc.NodeInput]:
        return [
            rbc.NodeInput(
                name="latent",
                type="latent",
                required=True,
                description="Latent tensor from KSampler",
            ),
        ]

    @classmethod
    def get_outputs(cls) -> List[rbc.NodeOutput]:
        return [
            rbc.NodeOutput(
                name="image",
                type="image",
                description="Decoded image",
            )
        ]

    def execute(self) -> Dict[str, Any]:
        latent = self.get_input("latent")
        
        if latent is None:
            raise ValueError("Latent input is required")
        
        print("[VAEDecode] Decoding latent to image")
        
        # In simplified version, KSampler already outputs image
        # So we just pass it through
        if "image" in latent:
            return {
                "image": {
                    "data": latent["image"],
                    "width": latent.get("width", 512),
                    "height": latent.get("height", 512),
                    "format": latent.get("format", "PNG"),
                    "type": "image",
                }
            }
        else:
            raise ValueError("Latent does not contain image data")


@rbc.register_node
class SaveImageNode(rbc.RBCNode):
    """
    Save Image Node - Save generated image
    
    Similar to ComfyUI's SaveImage node, this saves the generated image
    and returns metadata about the saved file.
    """

    NODE_TYPE = "save_image"
    DISPLAY_NAME = "Save Image"
    CATEGORY = "Text2Image"
    DESCRIPTION = "Save generated image to file"

    @classmethod
    def get_inputs(cls) -> List[rbc.NodeInput]:
        return [
            rbc.NodeInput(
                name="image",
                type="image",
                required=True,
                description="Image to save from VAEDecode",
            ),
            rbc.NodeInput(
                name="filename_prefix",
                type="string",
                required=False,
                default="ComfyUI",
                description="Filename prefix",
            ),
        ]

    @classmethod
    def get_outputs(cls) -> List[rbc.NodeOutput]:
        return [
            rbc.NodeOutput(
                name="image_path",
                type="string",
                description="Path to saved image file",
            ),
            rbc.NodeOutput(
                name="image_url",
                type="string",
                description="URL or data URI of the image",
            ),
        ]

    def execute(self) -> Dict[str, Any]:
        import os
        from datetime import datetime
        
        image_data = self.get_input("image")
        filename_prefix = self.get_input("filename_prefix", "ComfyUI")
        
        if image_data is None:
            raise ValueError("Image input is required")
        
        # Extract image info
        image_base64 = image_data.get("data", "")
        width = image_data.get("width", 512)
        height = image_data.get("height", 512)
        format = image_data.get("format", "PNG")
        
        if not image_base64:
            raise ValueError("Image data is empty")
        
        print(f"[SaveImage] Saving image: {width}x{height}")
        
        # Create output directory
        output_dir = "output"
        os.makedirs(output_dir, exist_ok=True)
        
        # Generate filename
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"{filename_prefix}_{timestamp}.{format.lower()}"
        filepath = os.path.join(output_dir, filename)
        
        # Decode and save image
        image_bytes = base64.b64decode(image_base64)
        with open(filepath, "wb") as f:
            f.write(image_bytes)
        
        print(f"[SaveImage] ✓ Image saved to: {filepath}")
        
        # Create data URI for preview
        image_url = f"data:image/{format.lower()};base64,{image_base64}"
        
        return {
            "image_path": filepath,
            "image_url": image_url,
        }


@rbc.register_node
class Text2ImageNode(rbc.RBCNode):
    """
    Text2Image Node - Complete text-to-image pipeline
    
    This is a simplified all-in-one node that combines CLIP encoding,
    sampling, and decoding into a single node for easier use.
    Similar to ComfyUI's workflow but as a single node.
    """

    NODE_TYPE = "text2image"
    DISPLAY_NAME = "Text2Image"
    CATEGORY = "Text2Image"
    DESCRIPTION = "Generate image from text prompt using Stable Diffusion"

    def __init__(self, node_id: str, context: Optional[Any] = None):
        super().__init__(node_id, context)
        self._pipeline: Optional[Any] = None

    @classmethod
    def get_inputs(cls) -> List[rbc.NodeInput]:
        return [
            rbc.NodeInput(
                name="prompt",
                type="string",
                required=True,
                default="a beautiful landscape",
                description="Text prompt for image generation",
            ),
            rbc.NodeInput(
                name="negative_prompt",
                type="string",
                required=False,
                default="",
                description="Negative prompt (things to avoid)",
            ),
            rbc.NodeInput(
                name="width",
                type="integer",
                required=False,
                default=512,
                description="Image width",
            ),
            rbc.NodeInput(
                name="height",
                type="integer",
                required=False,
                default=512,
                description="Image height",
            ),
            rbc.NodeInput(
                name="seed",
                type="integer",
                required=False,
                default=-1,
                description="Random seed (-1 for random)",
            ),
            rbc.NodeInput(
                name="steps",
                type="integer",
                required=False,
                default=20,
                description="Number of diffusion steps",
            ),
            rbc.NodeInput(
                name="cfg_scale",
                type="number",
                required=False,
                default=7.0,
                description="Classifier-free guidance scale",
            ),
            rbc.NodeInput(
                name="model_name",
                type="string",
                required=False,
                default="runwayml/stable-diffusion-v1-5",
                description="Stable Diffusion model name",
            ),
        ]

    @classmethod
    def get_outputs(cls) -> List[rbc.NodeOutput]:
        return [
            rbc.NodeOutput(
                name="image",
                type="image",
                description="Generated image as base64 PNG",
            ),
            rbc.NodeOutput(
                name="image_url",
                type="string",
                description="Data URI of the generated image",
            ),
            rbc.NodeOutput(
                name="seed",
                type="integer",
                description="Random seed used for generation",
            ),
        ]

    def _get_pipeline(self, model_name: str):
        """Get or create the diffusion pipeline"""
        if not DIFFUSERS_AVAILABLE:
            raise RuntimeError(
                "diffusers library not available. "
                "Install it with: pip install diffusers transformers accelerate"
            )
        
        if self._pipeline is None or self._pipeline.model_name != model_name:
            print(f"[Text2Image] Loading model: {model_name}")
            self._pipeline = StableDiffusionPipeline.from_pretrained(
                model_name,
                torch_dtype=torch.float16 if torch.cuda.is_available() else torch.float32,
            )
            
            if torch.cuda.is_available():
                self._pipeline = self._pipeline.to("cuda")
                print("[Text2Image] Using CUDA")
            else:
                print("[Text2Image] Using CPU")
            
            # Use DPM++ scheduler for better quality
            self._pipeline.scheduler = DPMSolverMultistepScheduler.from_config(
                self._pipeline.scheduler.config
            )
            self._pipeline.model_name = model_name  # Store for comparison
        
        return self._pipeline

    def execute(self) -> Dict[str, Any]:
        if not DIFFUSERS_AVAILABLE:
            raise RuntimeError(
                "diffusers library not available. "
                "Install it with: pip install diffusers transformers accelerate"
            )
        
        prompt = self.get_input("prompt", "a beautiful landscape")
        negative_prompt = self.get_input("negative_prompt", "")
        width = int(self.get_input("width", 512))
        height = int(self.get_input("height", 512))
        seed = int(self.get_input("seed", -1))
        steps = int(self.get_input("steps", 20))
        cfg_scale = float(self.get_input("cfg_scale", 7.0))
        model_name = self.get_input("model_name", "runwayml/stable-diffusion-v1-5")
        
        if not prompt:
            raise ValueError("Prompt is required")
        
        print(f"[Text2Image] Generating image: {width}x{height}")
        print(f"[Text2Image] Prompt: {prompt}")
        if negative_prompt:
            print(f"[Text2Image] Negative: {negative_prompt}")
        print(f"[Text2Image] Steps: {steps}, CFG: {cfg_scale}, Seed: {seed}")
        
        # Get pipeline
        pipeline = self._get_pipeline(model_name)
        
        # Set seed
        if seed == -1:
            seed = torch.randint(0, 2**32, (1,)).item()
        generator = torch.Generator()
        if torch.cuda.is_available():
            generator = generator.manual_seed(seed)
        else:
            generator = generator.manual_seed(seed)
        
        # Generate image
        print("[Text2Image] Generating image...")
        with torch.no_grad():
            result = pipeline(
                prompt=prompt,
                negative_prompt=negative_prompt if negative_prompt else None,
                width=width,
                height=height,
                num_inference_steps=steps,
                guidance_scale=cfg_scale,
                generator=generator,
            )
            image = result.images[0]
        
        print("[Text2Image] ✓ Image generated")
        
        # Convert PIL image to base64 for storage/transmission
        buffer = io.BytesIO()
        image.save(buffer, format="PNG")
        image_bytes = buffer.getvalue()
        image_base64 = base64.b64encode(image_bytes).decode("utf-8")
        
        # Create data URI for preview
        image_url = f"data:image/png;base64,{image_base64}"
        
        return {
            "image": {
                "data": image_base64,
                "width": width,
                "height": height,
                "format": "PNG",
                "type": "image",
            },
            "image_url": image_url,
            "seed": seed,
        }

