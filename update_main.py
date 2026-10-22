import os

main_path = "lib/main.dart"
if os.path.exists(main_path):
    with open(main_path, "r", encoding="utf-8") as f:
        content = f.read()

    # 1. إضافة الاستيراد إذا لم يكن موجوداً
    import_statement = "import 'banner_ad_widget.dart';"
    if import_statement not in content:
        content = import_statement + "\n" + content
        print("✔ تم إضافة استيراد ملف الإعلان إلى main.dart")

    # 2. إضافة إعلان البانر داخل Scaffold إذا لم يكن موجوداً
    if "BannerAdWidget" not in content[content.find(import_statement) + len(import_statement):]:
        if "Scaffold(" in content and "bottomNavigationBar" not in content:
            content = content.replace("Scaffold(", "Scaffold(\n  bottomNavigationBar: const SafeArea(child: SizedBox(height: 50, child: BannerAdWidget())),\n  ", 1)
            print("✔ تم استهداف وعرض BannerAdWidget في أسفل الـ Scaffold بنجاح!")
        else:
            print("⚠ تم إضافة الاستيراد، لكن الـ Scaffold غير موجود بالصيغة المعتادة، يرجى التأكد منه.")
    else:
        print("✔ BannerAdWidget مستهدف بالفعل في الكود.")

    with open(main_path, "w", encoding="utf-8") as f:
        f.write(content)
else:
    print("❌ خطأ: لم يتم العثور على ملف lib/main.dart")
