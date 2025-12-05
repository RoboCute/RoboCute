from rbc_meta.utils_next.templates import CPP_STRUCT_TEMPLATE


result = CPP_STRUCT_TEMPLATE.substitute(ENUM_NAME="World")

print(result)
