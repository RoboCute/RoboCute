"""
Markdown Python Code Block Parser and Filter.

This module provides functionality to parse markdown text and extract
Python code blocks with optional filtering capabilities.
"""

import re
import os
from dataclasses import dataclass
import asyncio
from typing import List, Optional
from kaos.path import KaosPath
from kimi_agent_sdk import prompt
from samples.cli.executor import async_input

# do not write API key in public repo!!!
# os.environ["KIMI_API_KEY"] = KIMI_CODE_CONSOLE_API_KEY
os.environ["KIMI_BASE_URL"] = "https://api.kimi.com/coding/v1"
os.environ["KIMI_MODEL_NAME"] = 'kimi-v2.5'


@dataclass
class CodeBlock:
    """
    Represents a code block extracted from markdown.

    Attributes:
        language: The programming language identifier (e.g., 'python', 'bash').
        code: The actual code content without markdown syntax.
        line_start: The starting line number in the original text.
        line_end: The ending line number in the original text.
    """
    code: str
    line_start: int
    line_end: int


def parse_code_blocks(
    markdown_text: str
) -> List[CodeBlock]:
    """
    Parse markdown text to extract code blocks.

    Args:
        markdown_text: The markdown text to parse.
        language_filter: If specified, only return code blocks with this language.
            Use 'python' to filter for Python code blocks.

    Returns:
        A list of CodeBlock objects matching the filter criteria.
    """
    code_blocks: List[CodeBlock] = []
    lines = markdown_text.split('\n')

    # Pattern to match code block start: ```python or ```
    fence_pattern = re.compile(r'^```(\w*)\s*$')

    i = 0
    while i < len(lines):
        match = fence_pattern.match(lines[i])
        if match:
            start_line = i + 1
            code_lines = []
            i += 1

            # Collect lines until closing fence
            while i < len(lines) and not lines[i].strip().startswith('```'):
                code_lines.append(lines[i])
                i += 1

            end_line = i
            code = '\n'.join(code_lines)
            code_blocks.append(CodeBlock(
                code=code,
                line_start=start_line,
                line_end=end_line
            ))
        i += 1

    return code_blocks


def filter_python_code_blocks(markdown_text: str) -> List[CodeBlock]:
    """
    Extract only Python code blocks from markdown text.

    Args:
        markdown_text: The markdown text to parse.

    Returns:
        A list of CodeBlock objects containing Python code.
    """
    return parse_code_blocks(markdown_text, language_filter='python')


def merge_code_blocks(code_blocks: List[CodeBlock]) -> str:
    """
    Merge multiple code blocks into a single code string.

    Args:
        code_blocks: The list of code blocks to merge.

    Returns:
        A single string containing all code blocks separated by newlines.
    """
    return '\n'.join(block.code for block in code_blocks)




async def call_kimi_agent(prompt_str: str) -> str:
    """
    Call the Kimi agent with a prompt and return extracted Python code.

    Args:
        prompt_str: The prompt string to send to the Kimi agent.

    Returns:
        A string containing all Python code blocks extracted from the response,
        merged together with newlines separating them.
    """
    # Collect all text from the async generator
    response_text = ""
    async for message in prompt(
        prompt_str,
        work_dir=KaosPath('.'),
        yolo=True,
        final_message_only=True
    ):
        response_text += message.extract_text()
    # # Parse code blocks and return merged code
    # code_blocks = parse_code_blocks(response_text)
    # return merge_code_blocks(code_blocks)
    return response_text


def input_kimi_agent(prompt_str: str):
    generator = async_input(prompt_str)
    prompt_str = None
    while True:
        try:
            prompt_str = next(generator)
        except StopIteration:
            break
        if prompt_str is not None:
            break
        yield None
    result = asyncio.run(call_kimi_agent(prompt_str))
    print(result)
    code = merge_code_blocks(parse_code_blocks(result))
    lines = code.replace('\r\n', '\n').split('\n')
    for i in lines:
        yield i
