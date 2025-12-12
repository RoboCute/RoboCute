"""
Basic Builtin Nodes for RoboCute
"""

from typing import Dict, Any, List
import math
import robocute as rbc


@rbc.register_node
class PrintNode(rbc.RBCNode):
    """Print Out Given Data Node"""

    NODE_TYPE = "BASIC:OUTPUT"
    DISPLAY_NAME = "Print To Console"
    CATEGORY = "BASIC"
    DESCRIPTION = "Print a given value to console"

    @classmethod
    def get_inputs(cls) -> List[rbc.NodeInput]:
        return [
            rbc.NodeInput(
                name="input_string",
                type="string",
                require=False,
                default="",
                description="Input String to Print",
            )
        ]

    @classmethod
    def get_outputs(cls) -> List[rbc.NodeOutput]:
        return []

    def execute(self) -> Dict[str, Any]:
        input_string = str(self.get_input("input_string", ""))
        print(f"[Basic::PrintNode] {input_string}")
        return {}
