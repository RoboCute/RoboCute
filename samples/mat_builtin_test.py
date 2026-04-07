"""Test cases for OpenPBRInterface."""

from mat_builtin import (
    OpenPBRInterface,
    _clamp_01,
    _clamp_01_tuple,
    openpbr_get_weight_base,
    openpbr_set_weight_base,
    openpbr_get_weight_metallic,
    openpbr_set_weight_metallic,
    openpbr_get_weight_weight_tex,
    openpbr_set_weight_weight_tex,
    openpbr_get_weight_tex_swizzle,
    openpbr_set_weight_tex_swizzle,
    openpbr_get_geometry_opacity,
    openpbr_set_geometry_opacity,
    openpbr_get_geometry_opacity_tex,
    openpbr_set_geometry_opacity_tex,
    openpbr_get_geometry_normal_tex,
    openpbr_set_geometry_normal_tex,
    openpbr_get_specular_color,
    openpbr_set_specular_color,
    openpbr_get_specular_roughness,
    openpbr_set_specular_roughness,
    openpbr_get_base_albedo,
    openpbr_set_base_albedo,
    openpbr_get_base_albedo_tex,
    openpbr_set_base_albedo_tex,
    openpbr_get_emission_luminance,
    openpbr_set_emission_luminance,
    openpbr_get_emission_emission_tex,
    openpbr_set_emission_emission_tex,
    openpbr_get_uvs_scale,
    openpbr_set_uvs_scale,
    openpbr_get_uvs_offset,
    openpbr_set_uvs_offset,
    openpbr_get_geometry_thin_walled,
    openpbr_set_geometry_thin_walled,
    openpbr_get_geometry_nested_priority,
    openpbr_set_geometry_nested_priority,
    openpbr_dump_to_json,
    openpbr_load_from_json,
)
import pytest
import sys
from pathlib import Path

# Add parent directory to path to import mat_builtin
sys.path.insert(0, str(Path(__file__).parent))


def float_tuple_equal_proximate(a: tuple, b: tuple, epsilon: float = 1e-5) -> bool:
    if len(a) != len(b):
        return False
    return all(abs(x - y) < epsilon for x, y in zip(a, b))


def float_equal_proximate(a: float, b: float, epsilon: float = 1e-5) -> bool:
    return abs(a - b) < epsilon


@pytest.fixture
def interface():
    """Create a fresh OpenPBRInterface instance for each test."""
    obj = OpenPBRInterface()
    obj._data = {}
    return obj


class TestClampFunctions:
    """Test helper clamp functions."""

    def test_clamp_01_within_range(self):
        assert _clamp_01(0.5) == 0.5
        assert _clamp_01(0.0) == 0.0
        assert _clamp_01(1.0) == 1.0

    def test_clamp_01_below_range(self):
        assert _clamp_01(-0.5) == 0.0
        assert _clamp_01(-10.0) == 0.0

    def test_clamp_01_above_range(self):
        assert _clamp_01(1.5) == 1.0
        assert _clamp_01(10.0) == 1.0

    def test_clamp_01_tuple(self):
        assert _clamp_01_tuple((0.5, 1.5, -0.5)) == (0.5, 1.0, 0.0)
        assert _clamp_01_tuple((0.0, 0.5, 1.0)) == (0.0, 0.5, 1.0)


class TestFloatProperties:
    """Test float properties with default values and clamping."""

    def test_weight_base_default(self, interface):
        assert float_equal_proximate(openpbr_get_weight_base(interface), 1.0)

    def test_weight_base_set_get(self, interface):
        openpbr_set_weight_base(interface, 0.5)
        assert float_equal_proximate(openpbr_get_weight_base(interface), 0.5)

    def test_weight_base_clamping(self, interface):
        openpbr_set_weight_base(interface, -0.5)
        assert float_equal_proximate(openpbr_get_weight_base(interface), 0.0)
        openpbr_set_weight_base(interface, 1.5)
        assert float_equal_proximate(openpbr_get_weight_base(interface), 1.0)

    def test_weight_metallic_default(self, interface):
        assert float_equal_proximate(openpbr_get_weight_metallic(interface), 0.0)

    def test_weight_metallic_set_get(self, interface):
        openpbr_set_weight_metallic(interface, 0.8)
        assert float_equal_proximate(openpbr_get_weight_metallic(interface), 0.8)

    def test_specular_roughness_default(self, interface):
        assert float_equal_proximate(openpbr_get_specular_roughness(interface), 0.3)

    def test_specular_roughness_set_get(self, interface):
        openpbr_set_specular_roughness(interface, 0.5)
        assert float_equal_proximate(openpbr_get_specular_roughness(interface), 0.5)

    def test_geometry_opacity_default(self, interface):
        assert float_equal_proximate(openpbr_get_geometry_opacity(interface), 1.0)

    def test_geometry_opacity_set_get(self, interface):
        openpbr_set_geometry_opacity(interface, 0.5)
        assert float_equal_proximate(openpbr_get_geometry_opacity(interface), 0.5)


class TestTupleProperties:
    """Test tuple properties (float2, float3)."""

    def test_specular_color_default(self, interface):
        assert float_tuple_equal_proximate(
            openpbr_get_specular_color(interface), (1.0, 1.0, 1.0))

    def test_specular_color_set_get(self, interface):
        openpbr_set_specular_color(interface, (0.5, 0.3, 0.2))
        spec_color = openpbr_get_specular_color(interface)
        assert float_tuple_equal_proximate(spec_color, (0.5, 0.3, 0.2))

    def test_specular_color_clamping(self, interface):
        openpbr_set_specular_color(interface, (-0.5, 0.5, 1.5))
        assert float_tuple_equal_proximate(
            openpbr_get_specular_color(interface), (0.0, 0.5, 1.0))

    def test_base_albedo_default(self, interface):
        assert float_tuple_equal_proximate(
            openpbr_get_base_albedo(interface), (1.0, 1.0, 1.0))

    def test_base_albedo_set_get(self, interface):
        openpbr_set_base_albedo(interface, (0.8, 0.2, 0.4))
        assert float_tuple_equal_proximate(
            openpbr_get_base_albedo(interface), (0.8, 0.2, 0.4))

    def test_emission_luminance_default(self, interface):
        assert float_tuple_equal_proximate(
            openpbr_get_emission_luminance(interface), (0.0, 0.0, 0.0))

    def test_emission_luminance_set_get(self, interface):
        openpbr_set_emission_luminance(interface, (1.0, 0.5, 0.2))
        assert float_tuple_equal_proximate(
            openpbr_get_emission_luminance(interface), (1.0, 0.5, 0.2))

    def test_uvs_scale_default(self, interface):
        assert float_tuple_equal_proximate(openpbr_get_uvs_scale(interface), (1.0, 1.0))

    def test_uvs_scale_set_get(self, interface):
        openpbr_set_uvs_scale(interface, (2.0, 3.0))
        assert float_tuple_equal_proximate(openpbr_get_uvs_scale(interface), (2.0, 3.0))

    def test_uvs_offset_default(self, interface):
        assert float_tuple_equal_proximate(openpbr_get_uvs_offset(interface), (0.0, 0.0))

    def test_uvs_offset_set_get(self, interface):
        openpbr_set_uvs_offset(interface, (0.5, 0.5))
        assert float_tuple_equal_proximate(openpbr_get_uvs_offset(interface), (0.5, 0.5))


class TestBoolProperties:
    """Test boolean properties."""

    def test_geometry_thin_walled_default(self, interface):
        assert openpbr_get_geometry_thin_walled(interface) is True

    def test_geometry_thin_walled_set_get(self, interface):
        openpbr_set_geometry_thin_walled(interface, False)
        assert openpbr_get_geometry_thin_walled(interface) is False


class TestIntProperties:
    """Test integer properties."""

    def test_geometry_nested_priority_default(self, interface):
        assert openpbr_get_geometry_nested_priority(interface) == 0

    def test_geometry_nested_priority_set_get(self, interface):
        openpbr_set_geometry_nested_priority(interface, 5)
        assert openpbr_get_geometry_nested_priority(interface) == 5

    def test_weight_tex_swizzle_default(self, interface):
        assert openpbr_get_weight_tex_swizzle(interface) == 4294967295

    def test_weight_tex_swizzle_set_get(self, interface):
        openpbr_set_weight_tex_swizzle(interface, 3)
        assert openpbr_get_weight_tex_swizzle(interface) == 3


class TestTextureProperties:
    """Test texture resource properties."""

    def test_weight_weight_tex_default(self, interface):
        result = openpbr_get_weight_weight_tex(interface)
        # Default should be TextureResource(None)
        assert result._handle is None

    def test_geometry_opacity_tex_default(self, interface):
        result = openpbr_get_geometry_opacity_tex(interface)
        assert result._handle is None

    def test_geometry_normal_tex_default(self, interface):
        result = openpbr_get_geometry_normal_tex(interface)
        assert result._handle is None

    def test_base_albedo_tex_default(self, interface):
        result = openpbr_get_base_albedo_tex(interface)
        assert result._handle is None

    def test_emission_emission_tex_default(self, interface):
        result = openpbr_get_emission_emission_tex(interface)
        assert result._handle is None


class TestJsonSerialization:
    """Test JSON serialization and deserialization."""

    def test_load_from_json(self, interface):
        json_str = '{"weight_base": 0.7, "specular_roughness": 0.4}'
        openpbr_load_from_json(interface, json_str)
        assert float_equal_proximate(openpbr_get_weight_base(interface), 0.7)
        assert float_equal_proximate(openpbr_get_specular_roughness(interface), 0.4)

    def test_round_trip(self, interface):
        # Set some values
        openpbr_set_weight_base(interface, 0.6)
        openpbr_set_weight_metallic(interface, 0.3)
        openpbr_set_base_albedo(interface, (0.2, 0.4, 0.8))
        openpbr_set_geometry_thin_walled(interface, False)

        # Serialize
        json_str = openpbr_dump_to_json(interface)

        # Create new interface and load
        new_interface = OpenPBRInterface()
        new_interface._data = {}
        openpbr_load_from_json(new_interface, json_str)

        # Verify values match
        assert float_equal_proximate(openpbr_get_weight_base(new_interface), 0.6)
        assert float_equal_proximate(openpbr_get_weight_metallic(new_interface), 0.3)
        albedo = openpbr_get_base_albedo(new_interface)
        assert float_tuple_equal_proximate(albedo, (0.2, 0.4, 0.8))
        assert openpbr_get_geometry_thin_walled(new_interface) is False

    def test_load_from_json_removes_type(self, interface):
        json_str = '{"type": "pbr", "weight_base": 0.5}'
        openpbr_load_from_json(interface, json_str)
        # 'type' should be removed from internal data
        assert float_equal_proximate(openpbr_get_weight_base(interface), 0.5)


class TestEdgeCases:
    """Test edge cases and error conditions."""

    def test_empty_interface_has_empty_data(self):
        interface = OpenPBRInterface()
        # Access internal data to verify it starts empty
        interface._data = {}
        assert interface._data == {}

    def test_multiple_properties_independent(self, interface):
        # Set multiple different properties
        openpbr_set_weight_base(interface, 0.8)
        openpbr_set_weight_metallic(interface, 0.3)
        openpbr_set_specular_roughness(interface, 0.5)

        # Verify each is independent
        assert float_equal_proximate(openpbr_get_weight_base(interface), 0.8)
        assert float_equal_proximate(openpbr_get_weight_metallic(interface), 0.3)
        assert float_equal_proximate(openpbr_get_specular_roughness(interface), 0.5)


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
