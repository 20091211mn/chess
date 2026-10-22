import os

main_path = "lib/main.dart"
if os.path.exists(main_path):
    with open(main_path, "r", encoding="utf-8") as f:
        content = f.read()

    import_statement = "import 'banner_ad_widget.dart';"
    if import_statement not in content:
        content = import_statement + "\n" + content
        print("Import added successfully to main.dart")

    if "BannerAdWidget" not in content[content.find(import_statement) + len(import_statement):]:
        if "Scaffold(" in content and "bottomNavigationBar" not in content:
            content = content.replace("Scaffold(", "Scaffold(\n  bottomNavigationBar: const SafeArea(child: SizedBox(height: 50, child: BannerAdWidget())),\n  ", 1)
            print("BannerAdWidget injected into Scaffold successfully!")
        else:
            print("Scaffold not found in standard format.")
    else:
        print("BannerAdWidget already exists in code.")

    with open(main_path, "w", encoding="utf-8") as f:
        f.write(content)
else:
    print("Error: lib/main.dart not found.")
