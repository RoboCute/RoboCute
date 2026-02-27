import json
from typing import Dict, Any, Optional
import robocute.rbc_ext as re
import robocute.rbc_ext.luisa as lc


class OpenPBRInterface:

    def ___init__(self):
        """Initialize an empty OpenPBR material interface."""
        self._data: Dict[str, Any] = {}
        self._data['type'] = 'pbr'


def _clamp_01(value: float) -> float:
    """Clamp a float value to [0.0, 1.0] range."""
    return max(0.0, min(1.0, value))


def _clamp_01_tuple(values: tuple) -> tuple:
    """Clamp each element in a tuple to [0.0, 1.0] range."""
    return tuple(max(0.0, min(1.0, v)) for v in values)

def openpbr_get_weight_base(self: OpenPBRInterface) -> float:
    """Get the base weight of the material."""
    return self._data.get('weight_base', 1.0)


def openpbr_set_weight_base(self: OpenPBRInterface, value: float) -> None:
    """Set the base weight of the material."""
    self._data['weight_base'] = _clamp_01(value)


def openpbr_get_weight_diffuse_roughness(self: OpenPBRInterface) -> float:
    """Get the diffuse roughness weight."""
    return self._data.get('weight_diffuse_roughness', 0.0)


def openpbr_set_weight_diffuse_roughness(self: OpenPBRInterface, value: float) -> None:
    """Set the diffuse roughness weight."""
    self._data['weight_diffuse_roughness'] = _clamp_01(value)


def openpbr_get_weight_specular(self: OpenPBRInterface) -> float:
    """Get the specular weight."""
    return self._data.get('weight_specular', 1.0)


def openpbr_set_weight_specular(self: OpenPBRInterface, value: float) -> None:
    """Set the specular weight."""
    self._data['weight_specular'] = _clamp_01(value)


def openpbr_get_weight_metallic(self: OpenPBRInterface) -> float:
    """Get the metallic weight."""
    return self._data.get('weight_metallic', 0.0)


def openpbr_set_weight_metallic(self: OpenPBRInterface, value: float) -> None:
    """Set the metallic weight."""
    self._data['weight_metallic'] = _clamp_01(value)


def openpbr_get_weight_metallic_roughness_tex(
    self: OpenPBRInterface,
) -> re.world.TextureResource:
    """Get the metallic roughness texture."""
    return self._data.get('weight_metallic_roughness_tex', re.world.TextureResource(None))


def openpbr_set_weight_metallic_roughness_tex(
    self: OpenPBRInterface, value: re.world.TextureResource
) -> None:
    """Set the metallic roughness texture."""
    self._data['weight_metallic_roughness_tex'] = value


def openpbr_get_weight_subsurface(self: OpenPBRInterface) -> float:
    """Get the subsurface weight."""
    return self._data.get('weight_subsurface', 0.0)


def openpbr_set_weight_subsurface(self: OpenPBRInterface, value: float) -> None:
    """Set the subsurface weight."""
    self._data['weight_subsurface'] = _clamp_01(value)


def openpbr_get_weight_transmission(self: OpenPBRInterface) -> float:
    """Get the transmission weight."""
    return self._data.get('weight_transmission', 0.0)


def openpbr_set_weight_transmission(self: OpenPBRInterface, value: float) -> None:
    """Set the transmission weight."""
    self._data['weight_transmission'] = _clamp_01(value)


def openpbr_get_weight_thin_film(self: OpenPBRInterface) -> float:
    """Get the thin film weight."""
    return self._data.get('weight_thin_film', 0.0)


def openpbr_set_weight_thin_film(self: OpenPBRInterface, value: float) -> None:
    """Set the thin film weight."""
    self._data['weight_thin_film'] = _clamp_01(value)


def openpbr_get_weight_fuzz(self: OpenPBRInterface) -> float:
    """Get the fuzz weight."""
    return self._data.get('weight_fuzz', 0.0)


def openpbr_set_weight_fuzz(self: OpenPBRInterface, value: float) -> None:
    """Set the fuzz weight."""
    self._data['weight_fuzz'] = _clamp_01(value)


def openpbr_get_weight_coat(self: OpenPBRInterface) -> float:
    """Get the coat weight."""
    return self._data.get('weight_coat', 0.0)


def openpbr_set_weight_coat(self: OpenPBRInterface, value: float) -> None:
    """Set the coat weight."""
    self._data['weight_coat'] = _clamp_01(value)


def openpbr_get_weight_diffraction(self: OpenPBRInterface) -> float:
    """Get the diffraction weight."""
    return self._data.get('weight_diffraction', 0.0)


def openpbr_set_weight_diffraction(self: OpenPBRInterface, value: float) -> None:
    """Set the diffraction weight."""
    self._data['weight_diffraction'] = _clamp_01(value)


def openpbr_get_geometry_cutout_threshold(self: OpenPBRInterface) -> float:
    """Get the geometry cutout threshold."""
    return self._data.get('geometry_cutout_threshold', 0.3)


def openpbr_set_geometry_cutout_threshold(self: OpenPBRInterface, value: float) -> None:
    """Set the geometry cutout threshold."""
    self._data['geometry_cutout_threshold'] = _clamp_01(value)


def openpbr_get_geometry_opacity(self: OpenPBRInterface) -> float:
    """Get the geometry opacity."""
    return self._data.get('geometry_opacity', 1.0)


def openpbr_set_geometry_opacity(self: OpenPBRInterface, value: float) -> None:
    """Set the geometry opacity."""
    self._data['geometry_opacity'] = _clamp_01(value)


def openpbr_get_geometry_opacity_tex(
    self: OpenPBRInterface,
) -> re.world.TextureResource:
    """Get the geometry opacity texture."""
    return self._data.get('geometry_opacity_tex', re.world.TextureResource(None))


def openpbr_set_geometry_opacity_tex(
    self: OpenPBRInterface, value: re.world.TextureResource
) -> None:
    """Set the geometry opacity texture."""
    self._data['geometry_opacity_tex'] = value


def openpbr_get_geometry_thickness(self: OpenPBRInterface) -> float:
    """Get the geometry thickness."""
    return self._data.get('geometry_thickness', 0.5)


def openpbr_set_geometry_thickness(self: OpenPBRInterface, value: float) -> None:
    """Set the geometry thickness."""
    self._data['geometry_thickness'] = value


def openpbr_get_geometry_thin_walled(self: OpenPBRInterface) -> bool:
    """Get whether the geometry is thin walled."""
    return self._data.get('geometry_thin_walled', True)


def openpbr_set_geometry_thin_walled(self: OpenPBRInterface, value: bool) -> None:
    """Set whether the geometry is thin walled."""
    self._data['geometry_thin_walled'] = value


def openpbr_get_geometry_nested_priority(self: OpenPBRInterface) -> int:
    """Get the geometry nested priority."""
    return self._data.get('geometry_nested_priority', 0)


def openpbr_set_geometry_nested_priority(self: OpenPBRInterface, value: int) -> None:
    """Set the geometry nested priority."""
    self._data['geometry_nested_priority'] = value


def openpbr_get_geometry_bump_scale(self: OpenPBRInterface) -> float:
    """Get the geometry bump scale."""
    return self._data.get('geometry_bump_scale', 1.0)


def openpbr_set_geometry_bump_scale(self: OpenPBRInterface, value: float) -> None:
    """Set the geometry bump scale."""
    self._data['geometry_bump_scale'] = value


def openpbr_get_geometry_normal_tex(
    self: OpenPBRInterface,
) -> re.world.TextureResource:
    """Get the geometry normal texture."""
    return self._data.get('geometry_normal_tex', re.world.TextureResource(None))


def openpbr_set_geometry_normal_tex(
    self: OpenPBRInterface, value: re.world.TextureResource
) -> None:
    """Set the geometry normal texture."""
    self._data['geometry_normal_tex'] = value


def openpbr_get_uvs_scale(self: OpenPBRInterface) -> tuple:
    """Get the UV scale as a float2 tuple."""
    return self._data.get('uvs_scale', (1.0, 1.0))


def openpbr_set_uvs_scale(self: OpenPBRInterface, value: tuple) -> None:
    """Set the UV scale from a float2 tuple."""
    self._data['uvs_scale'] = value


def openpbr_get_uvs_offset(self: OpenPBRInterface) -> tuple:
    """Get the UV offset as a float2 tuple."""
    return self._data.get('uvs_offset', (0.0, 0.0))


def openpbr_set_uvs_offset(self: OpenPBRInterface, value: tuple) -> None:
    """Set the UV offset from a float2 tuple."""
    self._data['uvs_offset'] = value


def openpbr_get_specular_color(self: OpenPBRInterface) -> tuple:
    """Get the specular color as a float3 tuple."""
    return self._data.get('specular_color', (1.0, 1.0, 1.0))


def openpbr_set_specular_color(self: OpenPBRInterface, value: tuple) -> None:
    """Set the specular color from a float3 tuple."""
    self._data['specular_color'] = _clamp_01_tuple(value)


def openpbr_get_specular_roughness(self: OpenPBRInterface) -> float:
    """Get the specular roughness."""
    return self._data.get('specular_roughness', 0.3)


def openpbr_set_specular_roughness(self: OpenPBRInterface, value: float) -> None:
    """Set the specular roughness."""
    self._data['specular_roughness'] = _clamp_01(value)


def openpbr_get_specular_roughness_anisotropy(self: OpenPBRInterface) -> float:
    """Get the specular roughness anisotropy."""
    return self._data.get('specular_roughness_anisotropy', 0.0)


def openpbr_set_specular_roughness_anisotropy(self: OpenPBRInterface, value: float) -> None:
    """Set the specular roughness anisotropy."""
    self._data['specular_roughness_anisotropy'] = _clamp_01(value)


def openpbr_get_specular_roughness_anisotropy_angle(self: OpenPBRInterface) -> float:
    """Get the specular roughness anisotropy angle."""
    return self._data.get('specular_roughness_anisotropy_angle', 0.0)


def openpbr_set_specular_roughness_anisotropy_angle(self: OpenPBRInterface, value: float) -> None:
    """Set the specular roughness anisotropy angle."""
    self._data['specular_roughness_anisotropy_angle'] = value


def openpbr_get_specular_anisotropy_level_tex(
    self: OpenPBRInterface,
) -> re.world.TextureResource:
    """Get the specular anisotropy level texture."""
    return self._data.get('specular_anisotropy_level_tex', re.world.TextureResource(None))


def openpbr_set_specular_anisotropy_level_tex(
    self: OpenPBRInterface, value: re.world.TextureResource
) -> None:
    """Set the specular anisotropy level texture."""
    self._data['specular_anisotropy_level_tex'] = value


def openpbr_get_specular_anisotropy_angle_tex(
    self: OpenPBRInterface,
) -> re.world.TextureResource:
    """Get the specular anisotropy angle texture."""
    return self._data.get('specular_anisotropy_angle_tex', re.world.TextureResource(None))


def openpbr_set_specular_anisotropy_angle_tex(
    self: OpenPBRInterface, value: re.world.TextureResource
) -> None:
    """Set the specular anisotropy angle texture."""
    self._data['specular_anisotropy_angle_tex'] = value


def openpbr_get_specular_ior(self: OpenPBRInterface) -> float:
    """Get the specular index of refraction."""
    return self._data.get('specular_ior', 1.5)


def openpbr_set_specular_ior(self: OpenPBRInterface, value: float) -> None:
    """Set the specular index of refraction."""
    self._data['specular_ior'] = value


def openpbr_get_emission_luminance(self: OpenPBRInterface) -> tuple:
    """Get the emission luminance as a float3 tuple."""
    return self._data.get('emission_luminance', (0.0, 0.0, 0.0))


def openpbr_set_emission_luminance(self: OpenPBRInterface, value: tuple) -> None:
    """Set the emission luminance from a float3 tuple."""
    self._data['emission_luminance'] = value


def openpbr_get_emission_emission_tex(
    self: OpenPBRInterface,
) -> re.world.TextureResource:
    """Get the emission texture."""
    return self._data.get('emission_emission_tex', re.world.TextureResource(None))


def openpbr_set_emission_emission_tex(
    self: OpenPBRInterface, value: re.world.TextureResource
) -> None:
    """Set the emission texture."""
    self._data['emission_emission_tex'] = value


def openpbr_get_base_albedo(self: OpenPBRInterface) -> tuple:
    """Get the base albedo color as a float3 tuple."""
    return self._data.get('base_albedo', (1.0, 1.0, 1.0))


def openpbr_set_base_albedo(self: OpenPBRInterface, value: tuple) -> None:
    """Set the base albedo color from a float3 tuple."""
    self._data['base_albedo'] = _clamp_01_tuple(value)


def openpbr_get_base_albedo_tex(
    self: OpenPBRInterface,
) -> re.world.TextureResource:
    """Get the base albedo texture."""
    return self._data.get('base_albedo_tex', re.world.TextureResource(None))


def openpbr_set_base_albedo_tex(
    self: OpenPBRInterface, value: re.world.TextureResource
) -> None:
    """Set the base albedo texture."""
    self._data['base_albedo_tex'] = value


def openpbr_get_subsurface_color(self: OpenPBRInterface) -> tuple:
    """Get the subsurface color as a float3 tuple."""
    return self._data.get('subsurface_color', (0.8, 0.8, 0.8))


def openpbr_set_subsurface_color(self: OpenPBRInterface, value: tuple) -> None:
    """Set the subsurface color from a float3 tuple."""
    self._data['subsurface_color'] = _clamp_01_tuple(value)


def openpbr_get_subsurface_radius(self: OpenPBRInterface) -> float:
    """Get the subsurface radius."""
    return self._data.get('subsurface_radius', 0.05)


def openpbr_set_subsurface_radius(self: OpenPBRInterface, value: float) -> None:
    """Set the subsurface radius."""
    self._data['subsurface_radius'] = value


def openpbr_get_subsurface_radius_scale(self: OpenPBRInterface) -> tuple:
    """Get the subsurface radius scale as a float3 tuple."""
    return self._data.get('subsurface_radius_scale', (1.0, 0.5, 0.25))


def openpbr_set_subsurface_radius_scale(self: OpenPBRInterface, value: tuple) -> None:
    """Set the subsurface radius scale from a float3 tuple."""
    self._data['subsurface_radius_scale'] = value


def openpbr_get_subsurface_scatter_anisotropy(self: OpenPBRInterface) -> float:
    """Get the subsurface scatter anisotropy."""
    return self._data.get('subsurface_scatter_anisotropy', 0.0)


def openpbr_set_subsurface_scatter_anisotropy(self: OpenPBRInterface, value: float) -> None:
    """Set the subsurface scatter anisotropy."""
    self._data['subsurface_scatter_anisotropy'] = _clamp_01(value)


def openpbr_get_transmission_color(self: OpenPBRInterface) -> tuple:
    """Get the transmission color as a float3 tuple."""
    return self._data.get('transmission_color', (1.0, 1.0, 1.0))


def openpbr_set_transmission_color(self: OpenPBRInterface, value: tuple) -> None:
    """Set the transmission color from a float3 tuple."""
    self._data['transmission_color'] = _clamp_01_tuple(value)


def openpbr_get_transmission_depth(self: OpenPBRInterface) -> float:
    """Get the transmission depth."""
    return self._data.get('transmission_depth', 0.0)


def openpbr_set_transmission_depth(self: OpenPBRInterface, value: float) -> None:
    """Set the transmission depth."""
    self._data['transmission_depth'] = value


def openpbr_get_transmission_scatter(self: OpenPBRInterface) -> tuple:
    """Get the transmission scatter as a float3 tuple."""
    return self._data.get('transmission_scatter', (0.0, 0.0, 0.0))


def openpbr_set_transmission_scatter(self: OpenPBRInterface, value: tuple) -> None:
    """Set the transmission scatter from a float3 tuple."""
    self._data['transmission_scatter'] = _clamp_01_tuple(value)


def openpbr_get_transmission_scatter_anisotropy(self: OpenPBRInterface) -> float:
    """Get the transmission scatter anisotropy."""
    return self._data.get('transmission_scatter_anisotropy', 0.0)


def openpbr_set_transmission_scatter_anisotropy(self: OpenPBRInterface, value: float) -> None:
    """Set the transmission scatter anisotropy."""
    self._data['transmission_scatter_anisotropy'] = _clamp_01(value)


def openpbr_get_transmission_dispersion_scale(self: OpenPBRInterface) -> float:
    """Get the transmission dispersion scale."""
    return self._data.get('transmission_dispersion_scale', 0.0)


def openpbr_set_transmission_dispersion_scale(self: OpenPBRInterface, value: float) -> None:
    """Set the transmission dispersion scale."""
    self._data['transmission_dispersion_scale'] = _clamp_01(value)


def openpbr_get_transmission_dispersion_abbe_number(self: OpenPBRInterface) -> float:
    """Get the transmission dispersion Abbe number."""
    return self._data.get('transmission_dispersion_abbe_number', 20.0)


def openpbr_set_transmission_dispersion_abbe_number(self: OpenPBRInterface, value: float) -> None:
    """Set the transmission dispersion Abbe number."""
    self._data['transmission_dispersion_abbe_number'] = value


def openpbr_get_coat_color(self: OpenPBRInterface) -> tuple:
    """Get the coat color as a float3 tuple."""
    return self._data.get('coat_color', (1.0, 1.0, 1.0))


def openpbr_set_coat_color(self: OpenPBRInterface, value: tuple) -> None:
    """Set the coat color from a float3 tuple."""
    self._data['coat_color'] = _clamp_01_tuple(value)


def openpbr_get_coat_roughness(self: OpenPBRInterface) -> float:
    """Get the coat roughness."""
    return self._data.get('coat_roughness', 0.0)


def openpbr_set_coat_roughness(self: OpenPBRInterface, value: float) -> None:
    """Set the coat roughness."""
    self._data['coat_roughness'] = _clamp_01(value)


def openpbr_get_coat_roughness_anisotropy(self: OpenPBRInterface) -> float:
    """Get the coat roughness anisotropy."""
    return self._data.get('coat_roughness_anisotropy', 0.0)


def openpbr_set_coat_roughness_anisotropy(self: OpenPBRInterface, value: float) -> None:
    """Set the coat roughness anisotropy."""
    self._data['coat_roughness_anisotropy'] = _clamp_01(value)


def openpbr_get_coat_roughness_anisotropy_angle(self: OpenPBRInterface) -> float:
    """Get the coat roughness anisotropy angle."""
    return self._data.get('coat_roughness_anisotropy_angle', 0.0)


def openpbr_set_coat_roughness_anisotropy_angle(self: OpenPBRInterface, value: float) -> None:
    """Set the coat roughness anisotropy angle."""
    self._data['coat_roughness_anisotropy_angle'] = value


def openpbr_get_coat_ior(self: OpenPBRInterface) -> float:
    """Get the coat index of refraction."""
    return self._data.get('coat_ior', 1.6)


def openpbr_set_coat_ior(self: OpenPBRInterface, value: float) -> None:
    """Set the coat index of refraction."""
    self._data['coat_ior'] = value


def openpbr_get_coat_darkening(self: OpenPBRInterface) -> float:
    """Get the coat darkening."""
    return self._data.get('coat_darkening', 1.0)


def openpbr_set_coat_darkening(self: OpenPBRInterface, value: float) -> None:
    """Set the coat darkening."""
    self._data['coat_darkening'] = _clamp_01(value)


def openpbr_get_coat_roughening(self: OpenPBRInterface) -> float:
    """Get the coat roughening."""
    return self._data.get('coat_roughening', 1.0)


def openpbr_set_coat_roughening(self: OpenPBRInterface, value: float) -> None:
    """Set the coat roughening."""
    self._data['coat_roughening'] = _clamp_01(value)


def openpbr_get_fuzz_color(self: OpenPBRInterface) -> tuple:
    """Get the fuzz color as a float3 tuple."""
    return self._data.get('fuzz_color', (1.0, 1.0, 1.0))


def openpbr_set_fuzz_color(self: OpenPBRInterface, value: tuple) -> None:
    """Set the fuzz color from a float3 tuple."""
    self._data['fuzz_color'] = _clamp_01_tuple(value)


def openpbr_get_fuzz_roughness(self: OpenPBRInterface) -> float:
    """Get the fuzz roughness."""
    return self._data.get('fuzz_roughness', 0.5)


def openpbr_set_fuzz_roughness(self: OpenPBRInterface, value: float) -> None:
    """Set the fuzz roughness."""
    self._data['fuzz_roughness'] = _clamp_01(value)


def openpbr_get_diffraction_color(self: OpenPBRInterface) -> tuple:
    """Get the diffraction color as a float3 tuple."""
    return self._data.get('diffraction_color', (1.0, 1.0, 1.0))


def openpbr_set_diffraction_color(self: OpenPBRInterface, value: tuple) -> None:
    """Set the diffraction color from a float3 tuple."""
    self._data['diffraction_color'] = _clamp_01_tuple(value)


def openpbr_get_diffraction_thickness(self: OpenPBRInterface) -> float:
    """Get the diffraction thickness."""
    return self._data.get('diffraction_thickness', 0.5)


def openpbr_set_diffraction_thickness(self: OpenPBRInterface, value: float) -> None:
    """Set the diffraction thickness."""
    self._data['diffraction_thickness'] = value


def openpbr_get_diffraction_inv_pitch_x(self: OpenPBRInterface) -> float:
    """Get the diffraction inverse pitch X."""
    return self._data.get('diffraction_inv_pitch_x', 1.0 / 3.0)


def openpbr_set_diffraction_inv_pitch_x(self: OpenPBRInterface, value: float) -> None:
    """Set the diffraction inverse pitch X."""
    self._data['diffraction_inv_pitch_x'] = value


def openpbr_get_diffraction_inv_pitch_y(self: OpenPBRInterface) -> float:
    """Get the diffraction inverse pitch Y."""
    return self._data.get('diffraction_inv_pitch_y', 0.0)


def openpbr_set_diffraction_inv_pitch_y(self: OpenPBRInterface, value: float) -> None:
    """Set the diffraction inverse pitch Y."""
    self._data['diffraction_inv_pitch_y'] = value


def openpbr_get_diffraction_angle(self: OpenPBRInterface) -> float:
    """Get the diffraction angle."""
    return self._data.get('diffraction_angle', 0.0)


def openpbr_set_diffraction_angle(self: OpenPBRInterface, value: float) -> None:
    """Set the diffraction angle."""
    self._data['diffraction_angle'] = value


def openpbr_get_diffraction_lobe_count(self: OpenPBRInterface) -> int:
    """Get the diffraction lobe count."""
    return self._data.get('diffraction_lobe_count', 5)


def openpbr_set_diffraction_lobe_count(self: OpenPBRInterface, value: int) -> None:
    """Set the diffraction lobe count."""
    self._data['diffraction_lobe_count'] = value


def openpbr_get_diffraction_type(self: OpenPBRInterface) -> int:
    """Get the diffraction type."""
    return self._data.get('diffraction_type', 1)


def openpbr_set_diffraction_type(self: OpenPBRInterface, value: int) -> None:
    """Set the diffraction type."""
    self._data['diffraction_type'] = value


def openpbr_get_thin_film_thickness(self: OpenPBRInterface) -> float:
    """Get the thin film thickness."""
    return self._data.get('thin_film_thickness', 0.5)


def openpbr_set_thin_film_thickness(self: OpenPBRInterface, value: float) -> None:
    """Set the thin film thickness."""
    self._data['thin_film_thickness'] = value


def openpbr_get_thin_film_ior(self: OpenPBRInterface) -> float:
    """Get the thin film index of refraction."""
    return self._data.get('thin_film_ior', 1.4)


def openpbr_set_thin_film_ior(self: OpenPBRInterface, value: float) -> None:
    """Set the thin film index of refraction."""
    self._data['thin_film_ior'] = value


def openpbr_dump_to_json(self: OpenPBRInterface) -> str:
    """Serialize the material data to a JSON string.

    Returns:
        A JSON string containing all material properties.
    """
    data = self._data.copy()
    data['type'] = 'pbr'
    return json.dumps(data, indent=2)


def openpbr_load_from_json(self: OpenPBRInterface, json_str: str) -> None:
    """Load material data from a JSON string.

    Args:
        json_str: A JSON string containing material properties.
    """
    self._data = json.loads(json_str)
