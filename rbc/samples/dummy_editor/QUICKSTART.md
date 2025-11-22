# Quick Start Guide

## 1. Build the Editor

```bash
# From project root
xmake build editor
```

## 2. Start Backend Server

In one terminal:

```bash
python main.py
```

You should see:

```
RBCNode System - ComfyUI Style Node Backend
Registered X node types:
...
Starting server at http://127.0.0.1:8000
```

## 3. Launch the Editor

In another terminal:

```bash
xmake run editor
```

## 4. Create a Simple Graph

Let's create a simple calculator: `(10 + 5) * 2 = 30`

### Step-by-step:

1. **Wait for Connection**: The console should show "Connected to backend server"
2. **Check Nodes**: Left panel should show node categories (输入, 数学, etc.)
3. **Add Nodes** (right-click in graph area → select nodes):
   - Add "输入数值" node (×3)
   - Add "加法" node (×1)
   - Add "乘法" node (×1)
   - Add "输出" node (×1)

4. **Configure Input Nodes**:
   - First input node: set value to `10`
   - Second input node: set value to `5`
   - Third input node: set value to `2`

5. **Connect Nodes**:
   - First input → 加法 (input 'a')
   - Second input → 加法 (input 'b')
   - 加法 output → 乘法 (input 'a')
   - Third input → 乘法 (input 'b')
   - 乘法 output → 输出 (input 'value')

6. **Execute**:
   - Click "Execute" button or press F5
   - Watch the console for execution log
   - Check "Results" tab to see output = 30

## Example: Text Concatenation

Try another example with text:

1. Add two "输入文本" nodes
   - First: "Hello "
   - Second: "World!"
2. Add one "文本连接" node
3. Add one "输出" node
4. Connect: input1 → concat 'a', input2 → concat 'b', concat → output
5. Execute and see result: "Hello World!"

## Saving Your Work

- **Save**: File → Save (Ctrl+S) → Choose location
- **Load**: File → Open (Ctrl+O) → Select saved .json file

## Troubleshooting

**"Disconnected" status**:
- Make sure backend is running
- Try Tools → Refresh Nodes

**No nodes appear**:
- Check console for errors
- Verify backend started successfully

**Execution fails**:
- Ensure all nodes are properly connected
- Check console for error details

## Next Steps

- Explore different node types in the palette
- Try more complex graphs
- Save and reload your graphs
- Check the full README.md for detailed documentation

