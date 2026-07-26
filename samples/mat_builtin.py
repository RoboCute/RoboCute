import json
from typing import Dict, Any, Optional
import robocute.rbc_ext as re
import robocute.rbc_ext._C.rbc_ext_c as rbc
import robocute.rbc_ext.luisa as lc


def _clamp_01(value: float) -> float:
    """Clamp a float value to [0.0, 1.0] range."""
    return max(0.0, min(1.0, value))


def _clamp_01_tuple(values: tuple) -> tuple:
    """Clamp each element in a tuple to [0.0, 1.0] range."""
    return tuple(max(0.0, min(1.0, v)) for v in values)


def _clamp_neg1_1(value: float) -> float:
    """Clamp a float value to [-1.0, 1.0] range."""
    return max(-1.0, min(1.0, value))


def _clamp_range(value: float, min_val: float, max_val: float) -> float:
    """Clamp a float value to [min_val, max_val] range."""
    return max(min_val, min(max_val, value))


def _clamp_int_range(value: int, min_val: int, max_val: int) -> int:
    """Clamp an int value to [min_val, max_val] range."""
    return max(min_val, min(max_val, value))


class ChannelSwizzle:
    """Manages texture channel swizzling for weight texture.
    
    Bit layout:
    - bits 0-2: metallic channel (0-7)
    - bits 3-5: specular-roughness channel (0-7)
    - bits 6-8: diffuse-roughness channel (0-7)
    - bits 9-11: base channel (0-7)
    """
    
    def __init__(
        self,
        metallic: int = 7,
        specular_roughness: int = 7,
        diffuse_roughness: int = 7,
        base: int = 7,
    ):
        """Initialize swizzle with channel indices (0-7, default 7 means unused)."""
        self._metallic = _clamp_int_range(metallic, 0, 7)
        self._specular_roughness = _clamp_int_range(specular_roughness, 0, 7)
        self._diffuse_roughness = _clamp_int_range(diffuse_roughness, 0, 7)
        self._base = _clamp_int_range(base, 0, 7)
    
    @property
    def metallic(self) -> int:
        """Get the metallic channel index (0-7)."""
        return self._metallic
    
    @metallic.setter
    def metallic(self, value: int) -> None:
        """Set the metallic channel index (0-7)."""
        self._metallic = _clamp_int_range(value, 0, 7)
    
    @property
    def specular_roughness(self) -> int:
        """Get the specular-roughness channel index (0-7)."""
        return self._specular_roughness
    
    @specular_roughness.setter
    def specular_roughness(self, value: int) -> None:
        """Set the specular-roughness channel index (0-7)."""
        self._specular_roughness = _clamp_int_range(value, 0, 7)
    
    @property
    def diffuse_roughness(self) -> int:
        """Get the diffuse-roughness channel index (0-7)."""
        return self._diffuse_roughness
    
    @diffuse_roughness.setter
    def diffuse_roughness(self, value: int) -> None:
        """Set the diffuse-roughness channel index (0-7)."""
        self._diffuse_roughness = _clamp_int_range(value, 0, 7)
    
    @property
    def base(self) -> int:
        """Get the base channel index (0-7)."""
        return self._base
    
    @base.setter
    def base(self, value: int) -> None:
        """Set the base channel index (0-7)."""
        self._base = _clamp_int_range(value, 0, 7)
    
    def to_int(self) -> int:
        """Pack swizzle into an integer."""
        return (
            self._metallic
            | (self._specular_roughness << 3)
            | (self._diffuse_roughness << 6)
            | (self._base << 9)
        )
    
    @classmethod
    def from_int(cls, value: int) -> "ChannelSwizzle":
        """Create ChannelSwizzle from a packed integer."""
        return cls(
            metallic=value & 0x7,
            specular_roughness=(value >> 3) & 0x7,
            diffuse_roughness=(value >> 6) & 0x7,
            base=(value >> 9) & 0x7,
        )


class OpenPBRInterface:
    def __init__(self, project: re.world.Project):
        """Initialize an empty OpenPBR material interface."""
        self._data: Dict[str, Any] = {}
        self._data['type'] = 'pbr'
        self._project = project

    def _guid_str_to_tex(self, guid_str: str) -> re.world.TextureResource:
        if guid_str is None:
            return None
        guid = rbc.GUID(guid_str)
        res = self._project.get_resource(guid, True)
        if res and res.type_name() == 'rbc::world::TextureResource':
            return re.world.TextureResource(res._handle)
        return None

    def _get_tex_with_uv(self, key: str) -> tuple:
        """Get texture resource and uv_index from data.
        
        Returns:
            Tuple of (TextureResource, uv_index). If no texture is set, returns (None, 0).
        """
        tex_data = self._data.get(key, None)
        if tex_data is None:
            return (None, 0)
        if isinstance(tex_data, list) and len(tex_data) == 2:
            guid_str, uv_index = tex_data
            tex = self._guid_str_to_tex(guid_str)
            return (tex, uv_index)
        # Handle legacy format (just guid string)
        if isinstance(tex_data, str):
            tex = self._guid_str_to_tex(tex_data)
            return (tex, 0)
        return (None, 0)

    def _set_tex_with_uv(self, key: str, value: re.world.TextureResource, uv_index: int = 0) -> None:
        """Set texture resource and uv_index to data.
        
        Args:
            key: The data key to store the texture under.
            value: The texture resource to store, or None to clear.
            uv_index: The UV index to use for the texture (default 0).
        """
        if value is None:
            self._data[key] = None
        else:
            self._data[key] = [str(value.guid()), uv_index]

    def get_weight_base(self) -> float:
        """Get the base weight of the material."""
        return self._data.get('weight_base', 1.0)

    def set_weight_base(self, value: float) -> None:
        """Set the base weight of the material."""
        self._data['weight_base'] = _clamp_01(value)

    def get_weight_diffuse_roughness(self) -> float:
        """Get the diffuse roughness weight."""
        return self._data.get('weight_diffuse_roughness', 0.0)

    def set_weight_diffuse_roughness(self, value: float) -> None:
        """Set the diffuse roughness weight."""
        self._data['weight_diffuse_roughness'] = _clamp_01(value)

    def get_weight_specular(self) -> float:
        """Get the specular weight."""
        return self._data.get('weight_specular', 1.0)

    def set_weight_specular(self, value: float) -> None:
        """Set the specular weight."""
        self._data['weight_specular'] = _clamp_01(value)

    def get_weight_metallic(self) -> float:
        """Get the metallic weight."""
        return self._data.get('weight_metallic', 0.0)

    def set_weight_metallic(self, value: float) -> None:
        """Set the metallic weight."""
        self._data['weight_metallic'] = _clamp_01(value)

    def get_weight_weight_tex(self) -> tuple:
        """Get the metallic roughness texture and its uv_index.
        
        Returns:
            Tuple of (TextureResource, uv_index). If no texture is set, returns (None, 0).
        """
        return self._get_tex_with_uv('weight_weight_tex')

    def set_weight_weight_tex(self, value: re.world.TextureResource, uv_index: int = 0) -> None:
        """Set the metallic roughness texture.
        
        Args:
            value: The texture resource to set.
            uv_index: The UV index to use for the texture (default 0).
        """
        self._set_tex_with_uv('weight_weight_tex', value, uv_index)

    def get_weight_tex_swizzle(self) -> ChannelSwizzle:
        """Get the weight texture swizzle."""
        value = self._data.get('weight_weight_tex_swizzle', None)
        if value is None:
            return ChannelSwizzle().to_int()
        return ChannelSwizzle.from_int(value)

    def set_weight_tex_swizzle(self, value: ChannelSwizzle) -> None:
        """Set the weight texture swizzle."""
        self._data['weight_weight_tex_swizzle'] = value.to_int()

    def get_weight_subsurface(self) -> float:
        """Get the subsurface weight."""
        return self._data.get('weight_subsurface', 0.0)

    def set_weight_subsurface(self, value: float) -> None:
        """Set the subsurface weight."""
        self._data['weight_subsurface'] = _clamp_01(value)

    def get_weight_transmission(self) -> float:
        """Get the transmission weight."""
        return self._data.get('weight_transmission', 0.0)

    def set_weight_transmission(self, value: float) -> None:
        """Set the transmission weight."""
        self._data['weight_transmission'] = _clamp_01(value)

    def get_weight_thin_film(self) -> float:
        """Get the thin film weight."""
        return self._data.get('weight_thin_film', 0.0)

    def set_weight_thin_film(self, value: float) -> None:
        """Set the thin film weight."""
        self._data['weight_thin_film'] = _clamp_01(value)

    def get_weight_fuzz(self) -> float:
        """Get the fuzz weight."""
        return self._data.get('weight_fuzz', 0.0)

    def set_weight_fuzz(self, value: float) -> None:
        """Set the fuzz weight."""
        self._data['weight_fuzz'] = _clamp_01(value)

    def get_weight_coat(self) -> float:
        """Get the coat weight."""
        return self._data.get('weight_coat', 0.0)

    def set_weight_coat(self, value: float) -> None:
        """Set the coat weight."""
        self._data['weight_coat'] = _clamp_01(value)

    def get_weight_diffraction(self) -> float:
        """Get the diffraction weight."""
        return self._data.get('weight_diffraction', 0.0)

    def set_weight_diffraction(self, value: float) -> None:
        """Set the diffraction weight."""
        self._data['weight_diffraction'] = _clamp_01(value)

    def get_weight_free_space_diffraction(self) -> float:
        """Get the free-space diffraction mixture weight."""
        return self._data.get('weight_free_space_diffraction', 0.0)

    def set_weight_free_space_diffraction(self, value: float) -> None:
        """Set the free-space diffraction mixture weight."""
        self._data['weight_free_space_diffraction'] = _clamp_01(value)

    def get_geometry_cutout_threshold(self) -> float:
        """Get the geometry cutout threshold."""
        return self._data.get('geometry_cutout_threshold', 0.3)

    def set_geometry_cutout_threshold(self, value: float) -> None:
        """Set the geometry cutout threshold."""
        self._data['geometry_cutout_threshold'] = _clamp_01(value)

    def get_geometry_opacity(self) -> float:
        """Get the geometry opacity."""
        return self._data.get('geometry_opacity', 1.0)

    def set_geometry_opacity(self, value: float) -> None:
        """Set the geometry opacity."""
        self._data['geometry_opacity'] = _clamp_01(value)

    def get_geometry_opacity_tex(self) -> tuple:
        """Get the geometry opacity texture and its uv_index.
        
        Returns:
            Tuple of (TextureResource, uv_index). If no texture is set, returns (None, 0).
        """
        return self._get_tex_with_uv('geometry_opacity_tex')

    def set_geometry_opacity_tex(self, value: re.world.TextureResource, uv_index: int = 0) -> None:
        """Set the geometry opacity texture.
        
        Args:
            value: The texture resource to set.
            uv_index: The UV index to use for the texture (default 0).
        """
        self._set_tex_with_uv('geometry_opacity_tex', value, uv_index)

    def get_geometry_thickness(self) -> float:
        """Get the geometry thickness."""
        return self._data.get('geometry_thickness', 0.5)

    def set_geometry_thickness(self, value: float) -> None:
        """Set the geometry thickness."""
        self._data['geometry_thickness'] = _clamp_range(value, 0.0, 50.0)

    def get_geometry_thin_walled(self) -> bool:
        """Get whether the geometry is thin walled."""
        return self._data.get('geometry_thin_walled', True)

    def set_geometry_thin_walled(self, value: bool) -> None:
        """Set whether the geometry is thin walled."""
        self._data['geometry_thin_walled'] = value

    def get_geometry_nested_priority(self) -> int:
        """Get the geometry nested priority."""
        return self._data.get('geometry_nested_priority', 0)

    def set_geometry_nested_priority(self, value: int) -> None:
        """Set the geometry nested priority."""
        self._data['geometry_nested_priority'] = _clamp_int_range(value, -5, 5)

    def get_geometry_bump_scale(self) -> float:
        """Get the geometry bump scale."""
        return self._data.get('geometry_bump_scale', 1.0)

    def set_geometry_bump_scale(self, value: float) -> None:
        """Set the geometry bump scale."""
        self._data['geometry_bump_scale'] = _clamp_range(value, 0.0, 5.0)

    def get_geometry_normal_tex(self) -> tuple:
        """Get the geometry normal texture and its uv_index.
        
        Returns:
            Tuple of (TextureResource, uv_index). If no texture is set, returns (None, 0).
        """
        return self._get_tex_with_uv('geometry_normal_tex')

    def set_geometry_normal_tex(self, value: re.world.TextureResource, uv_index: int = 0) -> None:
        """Set the geometry normal texture.
        
        Args:
            value: The texture resource to set.
            uv_index: The UV index to use for the texture (default 0).
        """
        self._set_tex_with_uv('geometry_normal_tex', value, uv_index)

    def get_uvs_scale(self) -> tuple:
        """Get the UV scale as a float2 tuple."""
        return self._data.get('uvs_scale', (1.0, 1.0))

    def set_uvs_scale(self, value: tuple) -> None:
        """Set the UV scale from a float2 tuple."""
        self._data['uvs_scale'] = value

    def get_uvs_offset(self) -> tuple:
        """Get the UV offset as a float2 tuple."""
        return self._data.get('uvs_offset', (0.0, 0.0))

    def set_uvs_offset(self, value: tuple) -> None:
        """Set the UV offset from a float2 tuple."""
        self._data['uvs_offset'] = value

    def get_specular_color(self) -> tuple:
        """Get the specular color as a float3 tuple."""
        return self._data.get('specular_color', (1.0, 1.0, 1.0))

    def set_specular_color(self, value: tuple) -> None:
        """Set the specular color from a float3 tuple."""
        self._data['specular_color'] = _clamp_01_tuple(value)

    def get_specular_roughness(self) -> float:
        """Get the specular roughness."""
        return self._data.get('specular_roughness', 0.3)

    def set_specular_roughness(self, value: float) -> None:
        """Set the specular roughness."""
        self._data['specular_roughness'] = _clamp_01(value)

    def get_specular_roughness_anisotropy(self) -> float:
        """Get the specular roughness anisotropy."""
        return self._data.get('specular_roughness_anisotropy', 0.0)

    def set_specular_roughness_anisotropy(self, value: float) -> None:
        """Set the specular roughness anisotropy."""
        self._data['specular_roughness_anisotropy'] = _clamp_01(value)

    def get_specular_roughness_anisotropy_angle(self) -> float:
        """Get the specular roughness anisotropy angle."""
        return self._data.get('specular_roughness_anisotropy_angle', 0.0)

    def set_specular_roughness_anisotropy_angle(self, value: float) -> None:
        """Set the specular roughness anisotropy angle."""
        self._data['specular_roughness_anisotropy_angle'] = _clamp_range(
            value, 0.0, 6.2831855)

    def get_specular_anisotropy_level_tex(self) -> tuple:
        """Get the specular anisotropy level texture and its uv_index.
        
        Returns:
            Tuple of (TextureResource, uv_index). If no texture is set, returns (None, 0).
        """
        return self._get_tex_with_uv('specular_anisotropy_level_tex')

    def set_specular_anisotropy_level_tex(self, value: re.world.TextureResource, uv_index: int = 0) -> None:
        """Set the specular anisotropy level texture.
        
        Args:
            value: The texture resource to set.
            uv_index: The UV index to use for the texture (default 0).
        """
        self._set_tex_with_uv('specular_anisotropy_level_tex', value, uv_index)

    def get_specular_anisotropy_angle_tex(self) -> tuple:
        """Get the specular anisotropy angle texture and its uv_index.
        
        Returns:
            Tuple of (TextureResource, uv_index). If no texture is set, returns (None, 0).
        """
        return self._get_tex_with_uv('specular_anisotropy_angle_tex')

    def set_specular_anisotropy_angle_tex(self, value: re.world.TextureResource, uv_index: int = 0) -> None:
        """Set the specular anisotropy angle texture.
        
        Args:
            value: The texture resource to set.
            uv_index: The UV index to use for the texture (default 0).
        """
        self._set_tex_with_uv('specular_anisotropy_angle_tex', value, uv_index)

    def get_specular_ior(self) -> float:
        """Get the specular index of refraction."""
        return self._data.get('specular_ior', 1.5)

    def set_specular_ior(self, value: float) -> None:
        """Set the specular index of refraction."""
        self._data['specular_ior'] = _clamp_range(value, 1.0, 50.0)

    def get_emission_luminance(self) -> tuple:
        """Get the emission luminance as a float3 tuple."""
        return self._data.get('emission_luminance', (0.0, 0.0, 0.0))

    def set_emission_luminance(self, value: tuple) -> None:
        """Set the emission luminance from a float3 tuple."""
        self._data['emission_luminance'] = value

    def get_emission_emission_tex(self) -> tuple:
        """Get the emission texture and its uv_index.
        
        Returns:
            Tuple of (TextureResource, uv_index). If no texture is set, returns (None, 0).
        """
        return self._get_tex_with_uv('emission_emission_tex')

    def set_emission_emission_tex(self, value: re.world.TextureResource, uv_index: int = 0) -> None:
        """Set the emission texture.
        
        Args:
            value: The texture resource to set.
            uv_index: The UV index to use for the texture (default 0).
        """
        self._set_tex_with_uv('emission_emission_tex', value, uv_index)

    def get_base_albedo(self) -> tuple:
        """Get the base albedo color as a float3 tuple."""
        return self._data.get('base_albedo', (1.0, 1.0, 1.0))

    def set_base_albedo(self, value: tuple) -> None:
        """Set the base albedo color from a float3 tuple."""
        self._data['base_albedo'] = _clamp_01_tuple(value)

    def get_base_albedo_tex(self) -> tuple:
        """Get the base albedo texture and its uv_index.
        
        Returns:
            Tuple of (TextureResource, uv_index). If no texture is set, returns (None, 0).
        """
        return self._get_tex_with_uv('base_albedo_tex')

    def set_base_albedo_tex(self, value: re.world.TextureResource, uv_index: int = 0) -> None:
        """Set the base albedo texture.
        
        Args:
            value: The texture resource to set.
            uv_index: The UV index to use for the texture (default 0).
        """
        self._set_tex_with_uv('base_albedo_tex', value, uv_index)

    def get_subsurface_color(self) -> tuple:
        """Get the subsurface color as a float3 tuple."""
        return self._data.get('subsurface_color', (0.8, 0.8, 0.8))

    def set_subsurface_color(self, value: tuple) -> None:
        """Set the subsurface color from a float3 tuple."""
        self._data['subsurface_color'] = _clamp_01_tuple(value)

    def get_subsurface_radius(self) -> float:
        """Get the subsurface radius."""
        return self._data.get('subsurface_radius', 0.05)

    def set_subsurface_radius(self, value: float) -> None:
        """Set the subsurface radius."""
        self._data['subsurface_radius'] = _clamp_range(value, 0.0, 1.0)

    def get_subsurface_radius_scale(self) -> tuple:
        """Get the subsurface radius scale as a float3 tuple."""
        return self._data.get('subsurface_radius_scale', (1.0, 0.5, 0.25))

    def set_subsurface_radius_scale(self, value: tuple) -> None:
        """Set the subsurface radius scale from a float3 tuple."""
        self._data['subsurface_radius_scale'] = value

    def get_subsurface_scatter_anisotropy(self) -> float:
        """Get the subsurface scatter anisotropy."""
        return self._data.get('subsurface_scatter_anisotropy', 0.0)

    def set_subsurface_scatter_anisotropy(self, value: float) -> None:
        """Set the subsurface scatter anisotropy."""
        self._data['subsurface_scatter_anisotropy'] = _clamp_neg1_1(value)

    def get_transmission_color(self) -> tuple:
        """Get the transmission color as a float3 tuple."""
        return self._data.get('transmission_color', (1.0, 1.0, 1.0))

    def set_transmission_color(self, value: tuple) -> None:
        """Set the transmission color from a float3 tuple."""
        self._data['transmission_color'] = _clamp_01_tuple(value)

    def get_transmission_depth(self) -> float:
        """Get the transmission depth."""
        return self._data.get('transmission_depth', 0.0)

    def set_transmission_depth(self, value: float) -> None:
        """Set the transmission depth."""
        self._data['transmission_depth'] = _clamp_range(value, 0.0, 50.0)

    def get_transmission_scatter(self) -> tuple:
        """Get the transmission scatter as a float3 tuple."""
        return self._data.get('transmission_scatter', (0.0, 0.0, 0.0))

    def set_transmission_scatter(self, value: tuple) -> None:
        """Set the transmission scatter from a float3 tuple."""
        self._data['transmission_scatter'] = _clamp_01_tuple(value)

    def get_transmission_scatter_anisotropy(self) -> float:
        """Get the transmission scatter anisotropy."""
        return self._data.get('transmission_scatter_anisotropy', 0.0)

    def set_transmission_scatter_anisotropy(self, value: float) -> None:
        """Set the transmission scatter anisotropy."""
        self._data['transmission_scatter_anisotropy'] = _clamp_neg1_1(value)

    def get_transmission_dispersion_scale(self) -> float:
        """Get the transmission dispersion scale."""
        return self._data.get('transmission_dispersion_scale', 0.0)

    def set_transmission_dispersion_scale(self, value: float) -> None:
        """Set the transmission dispersion scale."""
        self._data['transmission_dispersion_scale'] = _clamp_01(value)

    def get_transmission_dispersion_abbe_number(self) -> float:
        """Get the transmission dispersion Abbe number."""
        return self._data.get('transmission_dispersion_abbe_number', 20.0)

    def set_transmission_dispersion_abbe_number(self, value: float) -> None:
        """Set the transmission dispersion Abbe number."""
        self._data['transmission_dispersion_abbe_number'] = _clamp_range(
            value, 0.0, 100.0)

    def get_coat_color(self) -> tuple:
        """Get the coat color as a float3 tuple."""
        return self._data.get('coat_color', (1.0, 1.0, 1.0))

    def set_coat_color(self, value: tuple) -> None:
        """Set the coat color from a float3 tuple."""
        self._data['coat_color'] = _clamp_01_tuple(value)

    def get_coat_roughness(self) -> float:
        """Get the coat roughness."""
        return self._data.get('coat_roughness', 0.0)

    def set_coat_roughness(self, value: float) -> None:
        """Set the coat roughness."""
        self._data['coat_roughness'] = _clamp_01(value)

    def get_coat_roughness_anisotropy(self) -> float:
        """Get the coat roughness anisotropy."""
        return self._data.get('coat_roughness_anisotropy', 0.0)

    def set_coat_roughness_anisotropy(self, value: float) -> None:
        """Set the coat roughness anisotropy."""
        self._data['coat_roughness_anisotropy'] = _clamp_01(value)

    def get_coat_roughness_anisotropy_angle(self) -> float:
        """Get the coat roughness anisotropy angle."""
        return self._data.get('coat_roughness_anisotropy_angle', 0.0)

    def set_coat_roughness_anisotropy_angle(self, value: float) -> None:
        """Set the coat roughness anisotropy angle."""
        self._data['coat_roughness_anisotropy_angle'] = _clamp_range(
            value, 0.0, 6.2831855)

    def get_coat_ior(self) -> float:
        """Get the coat index of refraction."""
        return self._data.get('coat_ior', 1.6)

    def set_coat_ior(self, value: float) -> None:
        """Set the coat index of refraction."""
        self._data['coat_ior'] = _clamp_range(value, 1.0, 3.0)

    def get_coat_darkening(self) -> float:
        """Get the coat darkening."""
        return self._data.get('coat_darkening', 1.0)

    def set_coat_darkening(self, value: float) -> None:
        """Set the coat darkening."""
        self._data['coat_darkening'] = _clamp_01(value)

    def get_coat_roughening(self) -> float:
        """Get the coat roughening."""
        return self._data.get('coat_roughening', 1.0)

    def set_coat_roughening(self, value: float) -> None:
        """Set the coat roughening."""
        self._data['coat_roughening'] = _clamp_01(value)

    def get_fuzz_color(self) -> tuple:
        """Get the fuzz color as a float3 tuple."""
        return self._data.get('fuzz_color', (1.0, 1.0, 1.0))

    def set_fuzz_color(self, value: tuple) -> None:
        """Set the fuzz color from a float3 tuple."""
        self._data['fuzz_color'] = _clamp_01_tuple(value)

    def get_fuzz_roughness(self) -> float:
        """Get the fuzz roughness."""
        return self._data.get('fuzz_roughness', 0.5)

    def set_fuzz_roughness(self, value: float) -> None:
        """Set the fuzz roughness."""
        self._data['fuzz_roughness'] = _clamp_01(value)

    def get_diffraction_color(self) -> tuple:
        """Get the diffraction color as a float3 tuple."""
        return self._data.get('diffraction_color', (1.0, 1.0, 1.0))

    def set_diffraction_color(self, value: tuple) -> None:
        """Set the diffraction color from a float3 tuple."""
        self._data['diffraction_color'] = _clamp_01_tuple(value)

    def get_diffraction_thickness(self) -> float:
        """Get the diffraction thickness."""
        return self._data.get('diffraction_thickness', 0.5)

    def set_diffraction_thickness(self, value: float) -> None:
        """Set the diffraction thickness."""
        self._data['diffraction_thickness'] = _clamp_range(value, 0.0, 2.0)

    def get_diffraction_inv_pitch_x(self) -> float:
        """Get the diffraction inverse pitch X."""
        return self._data.get('diffraction_inv_pitch_x', 1.0 / 3.0)

    def set_diffraction_inv_pitch_x(self, value: float) -> None:
        """Set the diffraction inverse pitch X."""
        self._data['diffraction_inv_pitch_x'] = _clamp_01(value)

    def get_diffraction_inv_pitch_y(self) -> float:
        """Get the diffraction inverse pitch Y."""
        return self._data.get('diffraction_inv_pitch_y', 0.0)

    def set_diffraction_inv_pitch_y(self, value: float) -> None:
        """Set the diffraction inverse pitch Y."""
        self._data['diffraction_inv_pitch_y'] = _clamp_01(value)

    def get_diffraction_angle(self) -> float:
        """Get the diffraction angle."""
        return self._data.get('diffraction_angle', 0.0)

    def set_diffraction_angle(self, value: float) -> None:
        """Set the diffraction angle."""
        self._data['diffraction_angle'] = _clamp_range(value, 0.0, 6.2831855)

    def get_diffraction_lobe_count(self) -> int:
        """Get the diffraction lobe count."""
        return self._data.get('diffraction_lobe_count', 5)

    def set_diffraction_lobe_count(self, value: int) -> None:
        """Set the diffraction lobe count."""
        self._data['diffraction_lobe_count'] = _clamp_int_range(value, 1, 7)

    def get_diffraction_type(self) -> int:
        """Get the diffraction type."""
        return self._data.get('diffraction_type', 1)

    def set_diffraction_type(self, value: int) -> None:
        """Set the diffraction type."""
        self._data['diffraction_type'] = _clamp_int_range(value, 0, 1)

    def get_thin_film_thickness(self) -> float:
        """Get the thin film thickness."""
        return self._data.get('thin_film_thickness', 0.5)

    def set_thin_film_thickness(self, value: float) -> None:
        """Set the thin film thickness."""
        self._data['thin_film_thickness'] = _clamp_range(value, 0.0, 2.0)

    def get_thin_film_ior(self) -> float:
        """Get the thin film index of refraction."""
        return self._data.get('thin_film_ior', 1.4)

    def set_thin_film_ior(self, value: float) -> None:
        """Set the thin film index of refraction."""
        self._data['thin_film_ior'] = _clamp_range(value, 1.0, 3.0)

    def dump_to_json(self) -> str:
        """Serialize the material data to a JSON string.

        Returns:
            A JSON string containing all material properties.
        """
        data = self._data.copy()
        data['type'] = 'pbr'
        return json.dumps(data, indent=2)

    def load_from_json(self, json_str: str) -> None:
        """Load material data from a JSON string.

        Args:
            json_str: A JSON string containing material properties.
        """
        self._data = json.loads(json_str)
