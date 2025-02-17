with section("format"):  # noqa: F821
    dangle_parens = True
    max_lines_hwrap = 0
    keyword_case = "upper"
    autosort = True

with section("lint"):  # noqa: F821
    # The formatter sometimes fails to fit the code into the line limit (C0301)
    # and can disagree with the linter regarding the indentation (C0307):
    disabled_codes = ["C0301", "C0307"]
