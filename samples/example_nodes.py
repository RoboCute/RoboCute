from typing import Dict, Any, List
from robocute.node_base import RBCNode, NodeInput, NodeOutput
from robocute.node_registry import register_node


@register_node
class InputNumberNode(RBCNode):
    """输入数值节点"""

    NODE_TYPE = "input_number"
    DISPLAY_NAME = "输入数值"
    CATEGORY = "输入"
    DESCRIPTION = "提供一个数值输入"

    @classmethod
    def get_inputs(cls) -> List[NodeInput]:
        return [
            NodeInput(
                name="value",
                type="number",
                required=True,
                default=0.0,
                description="数值",
            )
        ]

    @classmethod
    def get_outputs(cls) -> List[NodeOutput]:
        return [NodeOutput(name="output", type="number", description="输出数值")]

    def execute(self) -> Dict[str, Any]:
        value = self.get_input("value", 0.0)
        return {"output": float(value)}


@register_node
class InputTextNode(RBCNode):
    """输入文本节点"""

    NODE_TYPE = "input_text"
    DISPLAY_NAME = "输入文本"
    CATEGORY = "输入"
    DESCRIPTION = "提供一个文本输入"

    @classmethod
    def get_inputs(cls) -> List[NodeInput]:
        return [
            NodeInput(
                name="text",
                type="string",
                required=True,
                default="",
                description="文本内容",
            )
        ]

    @classmethod
    def get_outputs(cls) -> List[NodeOutput]:
        return [NodeOutput(name="output", type="string", description="输出文本")]

    def execute(self) -> Dict[str, Any]:
        text = self.get_input("text", "")
        return {"output": str(text)}


@register_node
class MathAddNode(RBCNode):
    """数学加法节点"""

    NODE_TYPE = "math_add"
    DISPLAY_NAME = "加法"
    CATEGORY = "数学"
    DESCRIPTION = "计算两个数的和"

    @classmethod
    def get_inputs(cls) -> List[NodeInput]:
        return [
            NodeInput(
                name="a",
                type="number",
                required=True,
                default=0.0,
                description="第一个数",
            ),
            NodeInput(
                name="b",
                type="number",
                required=True,
                default=0.0,
                description="第二个数",
            ),
        ]

    @classmethod
    def get_outputs(cls) -> List[NodeOutput]:
        return [NodeOutput(name="result", type="number", description="和")]

    def execute(self) -> Dict[str, Any]:
        a = float(self.get_input("a", 0.0))
        b = float(self.get_input("b", 0.0))
        return {"result": a + b}


@register_node
class MathSubtractNode(RBCNode):
    """数学减法节点"""

    NODE_TYPE = "math_subtract"
    DISPLAY_NAME = "减法"
    CATEGORY = "数学"
    DESCRIPTION = "计算两个数的差"

    @classmethod
    def get_inputs(cls) -> List[NodeInput]:
        return [
            NodeInput(
                name="a",
                type="number",
                required=True,
                default=0.0,
                description="被减数",
            ),
            NodeInput(
                name="b", type="number", required=True, default=0.0, description="减数"
            ),
        ]

    @classmethod
    def get_outputs(cls) -> List[NodeOutput]:
        return [NodeOutput(name="result", type="number", description="差")]

    def execute(self) -> Dict[str, Any]:
        a = float(self.get_input("a", 0.0))
        b = float(self.get_input("b", 0.0))
        return {"result": a - b}


@register_node
class MathMultiplyNode(RBCNode):
    """数学乘法节点"""

    NODE_TYPE = "math_multiply"
    DISPLAY_NAME = "乘法"
    CATEGORY = "数学"
    DESCRIPTION = "计算两个数的积"

    @classmethod
    def get_inputs(cls) -> List[NodeInput]:
        return [
            NodeInput(
                name="a",
                type="number",
                required=True,
                default=1.0,
                description="第一个数",
            ),
            NodeInput(
                name="b",
                type="number",
                required=True,
                default=1.0,
                description="第二个数",
            ),
        ]

    @classmethod
    def get_outputs(cls) -> List[NodeOutput]:
        return [NodeOutput(name="result", type="number", description="积")]

    def execute(self) -> Dict[str, Any]:
        a = float(self.get_input("a", 1.0))
        b = float(self.get_input("b", 1.0))
        return {"result": a * b}


@register_node
class MathDivideNode(RBCNode):
    """数学除法节点"""

    NODE_TYPE = "math_divide"
    DISPLAY_NAME = "除法"
    CATEGORY = "数学"
    DESCRIPTION = "计算两个数的商"

    @classmethod
    def get_inputs(cls) -> List[NodeInput]:
        return [
            NodeInput(
                name="a",
                type="number",
                required=True,
                default=1.0,
                description="被除数",
            ),
            NodeInput(
                name="b", type="number", required=True, default=1.0, description="除数"
            ),
        ]

    @classmethod
    def get_outputs(cls) -> List[NodeOutput]:
        return [NodeOutput(name="result", type="number", description="商")]

    def execute(self) -> Dict[str, Any]:
        a = float(self.get_input("a", 1.0))
        b = float(self.get_input("b", 1.0))

        if b == 0:
            raise ValueError("Cannot divide by zero")

        return {"result": a / b}


@register_node
class TextConcatNode(RBCNode):
    """文本连接节点"""

    NODE_TYPE = "text_concat"
    DISPLAY_NAME = "文本连接"
    CATEGORY = "文本"
    DESCRIPTION = "连接两个文本字符串"

    @classmethod
    def get_inputs(cls) -> List[NodeInput]:
        return [
            NodeInput(
                name="text1",
                type="string",
                required=True,
                default="",
                description="第一个文本",
            ),
            NodeInput(
                name="text2",
                type="string",
                required=True,
                default="",
                description="第二个文本",
            ),
            NodeInput(
                name="separator",
                type="string",
                required=False,
                default="",
                description="分隔符",
            ),
        ]

    @classmethod
    def get_outputs(cls) -> List[NodeOutput]:
        return [NodeOutput(name="output", type="string", description="连接后的文本")]

    def execute(self) -> Dict[str, Any]:
        text1 = str(self.get_input("text1", ""))
        text2 = str(self.get_input("text2", ""))
        separator = str(self.get_input("separator", ""))

        result = text1 + separator + text2
        return {"output": result}


@register_node
class OutputNode(RBCNode):
    """输出节点"""

    NODE_TYPE = "output"
    DISPLAY_NAME = "输出"
    CATEGORY = "输出"
    DESCRIPTION = "收集和显示输出结果"

    @classmethod
    def get_inputs(cls) -> List[NodeInput]:
        return [
            NodeInput(name="value", type="any", required=True, description="要输出的值")
        ]

    @classmethod
    def get_outputs(cls) -> List[NodeOutput]:
        return [NodeOutput(name="output", type="any", description="输出值")]

    def execute(self) -> Dict[str, Any]:
        value = self.get_input("value")
        print(f"[Output {self.node_id}]: {value}")
        return {"output": value}


@register_node
class NumberToTextNode(RBCNode):
    """数值转文本节点"""

    NODE_TYPE = "number_to_text"
    DISPLAY_NAME = "数值转文本"
    CATEGORY = "转换"
    DESCRIPTION = "将数值转换为文本"

    @classmethod
    def get_inputs(cls) -> List[NodeInput]:
        return [
            NodeInput(
                name="number",
                type="number",
                required=True,
                default=0.0,
                description="数值",
            )
        ]

    @classmethod
    def get_outputs(cls) -> List[NodeOutput]:
        return [NodeOutput(name="text", type="string", description="文本")]

    def execute(self) -> Dict[str, Any]:
        number = self.get_input("number", 0.0)
        return {"text": str(number)}


@register_node
class PrintNode(RBCNode):
    """打印节点"""

    NODE_TYPE = "print"
    DISPLAY_NAME = "打印"
    CATEGORY = "调试"
    DESCRIPTION = "打印值到控制台"

    @classmethod
    def get_inputs(cls) -> List[NodeInput]:
        return [
            NodeInput(
                name="value", type="any", required=True, description="要打印的值"
            ),
            NodeInput(
                name="label",
                type="string",
                required=False,
                default="",
                description="标签",
            ),
        ]

    @classmethod
    def get_outputs(cls) -> List[NodeOutput]:
        return [NodeOutput(name="passthrough", type="any", description="传递输入值")]

    def execute(self) -> Dict[str, Any]:
        value = self.get_input("value")
        label = self.get_input("label", "")

        if label:
            print(f"[Print {self.node_id}] {label}: {value}")
        else:
            print(f"[Print {self.node_id}]: {value}")

        return {"passthrough": value}
