import subprocess
import re
import os
import difflib


for file in os.listdir('tests'):
    match = re.match(r'test(\d+)', file)
    if match and os.path.isfile(f"tests/out{match.group(1)}"):
        lex_output = subprocess.run(['./main', f"tests/{file}"], capture_output=True, text=True).stdout
        test_output = open(f"tests/out{match.group(1)}").read()

        if lex_output == test_output:
            print(f"Test {match.group(1)} passed")
        else:
            print(f"Test {match.group(1)} possibly failed")
            diff = difflib.unified_diff(
                lex_output.splitlines(keepends=True),
                test_output.splitlines(keepends=True),
                fromfile='test_output',
                tofile='lex_output',
                lineterm='',
                n=1
            )
            print(''.join(diff))

