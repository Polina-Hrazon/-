#define NOMINMAX
#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <string>
#include <fstream>
#include <iomanip>
#include <vector>
#include <map>
#include <limits>
#include <ctime>
#include <cstdlib>
#include <cctype>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;

// ======================================================================
// СПІЛЬНІ ДАНІ БІБЛІОТЕКИ
// ЄДИНЕ ДЖЕРЕЛО ІСТОРІЇ ОПЕРАЦІЙ
// ======================================================================

namespace LibraryData {

    enum class OperationType {
        Issue,
        Return
    };

    // Операція зберігає тільки ідентифікатори, дату та тип.
    // Назви книг і читачів тут не дублюються.
    struct Operation {
        string date;
        int bookId;
        int readerId;
        OperationType type;
    };

    vector<Operation> operations;

    // Поточна дата у форматі YYYY-MM-DD
    string todayISO() {
        time_t now = time(nullptr);
        tm* local = localtime(&now);

        char buffer[11];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d", local);

        return buffer;
    }

} // namespace LibraryData


// ======================================================================
// ПОШУК КНИГ
// ВІДПОВІДАЛЬНА: Золотих Катерина
// ======================================================================

namespace Search {

    // Єдина структура книги для всіх модулів.
    // Категорія також зберігається тут.
    struct Book {
        int id;
        string title;
        string author;
        int year;
        string keywords;
        int available;
        string category;
    };

    // Єдиний каталог книг для всієї програми.
    vector<Book> books = {
        { 1, "Чистий код", "Р. Мартін", 2019,
          "програмування код якість", 1, "Програмування" },

        { 2, "Основи програмної інженерії", "І. Соммервіль", 2018,
          "інженерія програмування", 3, "Програмування" },

        { 3, "UML. Керівництво користувача", "Г. Буч", 2015,
          "uml моделювання", 0, "Програмування" },

        { 4, "Мова програмування C++", "Б. Страуструп", 2020,
          "c++ програмування мова", 2, "Програмування" },

        { 5, "Алгоритми та структури даних", "Т. Кормен", 2017,
          "алгоритми структури", 1, "Програмування" },

        { 6, "Бази даних. Повний курс", "Г. Гарсіа-Молина", 2016,
          "sql бази даних", 0, "Робота з даними" },

        { 7, "Тестування програмного забезпечення", "Г. Майерс", 2014,
          "тестування якість", 2, "Кар'єра та навчання" },

        { 8, "Git для командної розробки", "С. Чакон", 2021,
          "git команда", 2, "Програмування" }
    };

    // Повертає індекс книги за її ID.
    int findBook(int id) {
        for (int i = 0; i < (int)books.size(); i++) {
            if (books[i].id == id)
                return i;
        }

        return -1;
    }

    string statusOf(const Book& book) {
        return book.available > 0 ? "Доступна" : "Видана";
    }

    struct Query {
        string title;
        string author;
        string keyword;
    };

    // Введення рядка з підтримкою UTF-8.
    string readLine(const string& prompt) {
        cout << prompt << flush;

#ifdef _WIN32
        wchar_t buffer[512];
        DWORD length = 0;

        ReadConsoleW(
            GetStdHandle(STD_INPUT_HANDLE),
            buffer,
            511,
            &length,
            nullptr
        );

        while (length > 0 &&
            (buffer[length - 1] == L'\r' ||
                buffer[length - 1] == L'\n')) {
            length--;
        }

        int utf8Length = WideCharToMultiByte(
            CP_UTF8, 0, buffer, length,
            nullptr, 0, nullptr, nullptr
        );

        string result(utf8Length, '\0');

        if (utf8Length > 0) {
            WideCharToMultiByte(
                CP_UTF8, 0, buffer, length,
                &result[0], utf8Length, nullptr, nullptr
            );
        }

        return result;
#else
        string result;
        getline(cin, result);
        return result;
#endif
    }

    int readNumber(const string& prompt) {
        while (true) {
            string input = readLine(prompt);

            try {
                size_t position;
                int value = stoi(input, &position);

                if (position == input.size() && value >= 0)
                    return value;
            }
            catch (...) {
            }

            cout << "Помилка: введіть невід'ємне ціле число.\n";
        }
    }

    // Перетворення ASCII та української кирилиці на нижній регістр.
    string toLowerUtf8(const string& s) {
        string result;

        for (size_t i = 0; i < s.size(); i++) {
            unsigned char c = s[i];

            if (c < 0x80) {
                result += (char)tolower(c);
            }
            else if (c == 0xD0 && i + 1 < s.size()) {
                unsigned char d = s[++i];

                if (d >= 0x90 && d <= 0x9F) {
                    result += (char)0xD0;
                    result += (char)(d + 0x20);
                }
                else if (d >= 0xA0 && d <= 0xAF) {
                    result += (char)0xD1;
                    result += (char)(d - 0x20);
                }
                else if (d == 0x84) {
                    result += (char)0xD1;
                    result += (char)0x94;
                }
                else if (d == 0x86) {
                    result += (char)0xD1;
                    result += (char)0x96;
                }
                else if (d == 0x87) {
                    result += (char)0xD1;
                    result += (char)0x97;
                }
                else {
                    result += (char)c;
                    result += (char)d;
                }
            }
            else if (c == 0xD2 && i + 1 < s.size() &&
                (unsigned char)s[i + 1] == 0x90) {
                i++;
                result += (char)0xD2;
                result += (char)0x91;
            }
            else {
                result += (char)c;
            }
        }

        return result;
    }

    bool contains(const string& text, const string& part) {
        return toLowerUtf8(text).find(toLowerUtf8(part))
            != string::npos;
    }

    Query inputQuery() {
        Query query;

        cout << "\nВведіть параметри пошуку (Enter — пропустити поле)\n";

        query.title = readLine("Назва: ");
        query.author = readLine("Автор: ");
        query.keyword = readLine("Ключове слово: ");

        return query;
    }

    string trim(const string & string);

    // Повертає індекси книг, що відповідають запиту.
    vector<int> searchBooks(const Query& query) {
        vector<int> found;
        // ВИПРАВЛЕННЯ ДЕФЕКТУ №1: обрізаємо пробіли на початку та в кінці полів запиту
        string title = trim(query.title);
        string author = trim(query.author);
        string keyword = trim(query.keyword);

        // ВИПРАВЛЕННЯ ДЕФЕКТУ №2: якщо всі поля порожні — повертаємо порожній вектор результатів
        if (title.empty() && author.empty() && keyword.empty()) {
            cout << "Введіть хоча б один параметр пошуку." << endl;
            return found;
        }
            for (int i = 0; i < (int)books.size(); i++)
            {
                if (!title.empty() &&
                    !contains(books[i].title, title))
                    continue;

                if (!author.empty() &&
                    !contains(books[i].author, author))
                    continue;

                if (!keyword.empty() &&
                    !contains(books[i].keywords, keyword))
                    continue;

                found.push_back(i);
            }

        return found;
    }

    void printResults(const vector<int>& found) {
        cout << "\nЗнайдено книг: " << found.size() << "\n";

        for (int index : found) {
            const Book& book = books[index];

            cout << book.id << ". " << book.title
                << " — " << book.author
                << " (" << statusOf(book) << ")\n";
        }
    }

    void printCard(const Book& book) {
        cout << "\n--- Картка книги ---\n";
        cout << "Номер: " << book.id << endl;
        cout << "Назва: " << book.title << endl;
        cout << "Автор: " << book.author << endl;
        cout << "Рік видання: " << book.year << endl;
        cout << "Ключові слова: " << book.keywords << endl;
        cout << "Категорія: " << book.category << endl;
        cout << "Статус: " << statusOf(book) << endl;
    }

    void run() {
        while (true) {
            cout << "\n1 - Пошук книги\n0 - Вихід\n";

            string choice = readLine("Ваш вибір: ");

            if (choice == "0")
                break;

            if (choice != "1") {
                cout << "Помилка: оберіть 0 або 1.\n";
                continue;
            }

            Query query = inputQuery();
            vector<int> found = searchBooks(query);

            if (found.empty()) {
                cout << "За вашим запитом нічого не знайдено.\n";
                continue;
            }

            printResults(found);

            int id = readNumber(
                "Номер книги для перегляду картки (0 - назад): "
            );

            if (id == 0)
                continue;

            int index = findBook(id);

            bool isFound = false;

            for (int foundIndex : found) {
                if (books[foundIndex].id == id) {
                    printCard(books[foundIndex]);
                    isFound = true;
                    break;
                }
            }

            if (!isFound)
                cout << "Помилка: серед знайдених книг такого номера немає.\n";
        }
    }

} // namespace Search


// ======================================================================
// БРОНЮВАННЯ КНИГ
// ВІДПОВІДАЛЬНИЙ: Дудай Владислав
// ======================================================================
namespace Booking {

    const int DAYS = 7;
    const int MAX_RES = 3;

    using Search::Book;
    using Search::books;

    // Категорія читача зберігається разом із його даними.
    struct Reader {
        int id;
        string name;
        int debt;
        string category;
    };

    struct Reservation {
        int readerId;
        int bookId;
        int day;
    };

    // Початковий список читачів не змінено.
    vector<Reader> readers = {
        { 1, "Золотих Катерина", 0, "Студенти" },
        { 2, "Андруневчин Вероніка", 20, "Студенти" },
        { 3, "Гразьон Поліна", 0, "Студенти" },
        { 4, "Дудай Владислав", 0, "Студенти" }
    };

    vector<Reservation> reservations = {
        { 3, 1, 2 }
    };

    int findBook(int id) {
        return Search::findBook(id);
    }

    int findReader(int id) {
        for (int i = 0; i < (int)readers.size(); i++) {
            if (readers[i].id == id)
                return i;
        }

        return -1;
    }

    int readInt(const string& prompt, int lo, int hi) {
        while (true) {
            cout << prompt;

            int value;

            if (cin >> value && value >= lo && value <= hi) {
                cin.ignore(
                    numeric_limits<streamsize>::max(), '\n'
                );

                return value;
            }

            if (cin.eof()) {
                cout << "\nВвід завершено.\n";
                exit(0);
            }

            cin.clear();
            cin.ignore(
                numeric_limits<streamsize>::max(), '\n'
            );

            cout << "Помилка: введіть ціле число від "
                << lo << " до " << hi << ".\n";
        }
    }

    int findReservation(int readerId, int bookId) {
        for (int i = 0; i < (int)reservations.size(); i++) {
            if (reservations[i].readerId == readerId &&
                reservations[i].bookId == bookId)
                return i;
        }

        return -1;
    }

    int countReaderReservations(int readerId) {
        int count = 0;

        for (const Reservation& reservation : reservations) {
            if (reservation.readerId == readerId)
                count++;
        }

        return count;
    }

    int countBookedOnDay(int bookId, int day) {
        int count = 0;

        for (const Reservation& reservation : reservations) {
            if (reservation.bookId == bookId &&
                reservation.day == day)
                count++;
        }

        return count;
    }

    bool isDayFree(int bookId, int day) {
        int bookIndex = findBook(bookId);

        if (bookIndex < 0)
            return false;

        return countBookedOnDay(bookId, day)
            < books[bookIndex].available;
    }

    string dateFromDay(int day) {
        time_t timeValue = time(nullptr)
            + day * 24 * 60 * 60;

        char buffer[16];

        strftime(
            buffer, sizeof(buffer), "%d.%m.%Y",
            localtime(&timeValue)
        );

        return buffer;
    }

    string checkBooking(int readerId, int bookId) {
        int readerIndex = findReader(readerId);
        int bookIndex = findBook(bookId);

        if (readerIndex < 0)
            return "Помилка: читача не знайдено.";

        if (bookIndex < 0)
            return "Помилка: книгу не знайдено.";

        if (readers[readerIndex].debt > 0) {
            return "Відмова: у читача є борг "
                + to_string(readers[readerIndex].debt) + " грн.";
        }

        if (findReservation(readerId, bookId) >= 0)
            return "Відмова: цю книгу читач уже забронював.";

        if (countReaderReservations(readerId) >= MAX_RES) {
            return "Відмова: досягнуто ліміт бронювань ("
                + to_string(MAX_RES) + ").";
        }

        if (books[bookIndex].available <= 0) {
            return "Відмова: книга недоступна для бронювання "
                "(немає вільних примірників).";
        }

        return "";
    }

    void printBooks() {
        cout << "\nКаталог книг:\n";

        for (const Book& book : books) {
            cout << book.id << ". " << book.title
                << " — " << book.author << ", " << book.year
                << " (вільних примірників: " << book.available << ")"
                << (book.available > 0 ? "" : " [недоступна]")
                << "\n";
        }
    }

    void printDays(int bookId) {
        cout << "\nДати отримання книги:\n";

        for (int day = 1; day <= DAYS; day++) {
            cout << day << ". " << dateFromDay(day)
                << (isDayFree(bookId, day) ? "" : " [зайнято]")
                << "\n";
        }
    }

    void printReservations(int readerId) {
        int readerIndex = findReader(readerId);

        if (readerIndex < 0) {
            cout << "Помилка: читача не знайдено.\n";
            return;
        }

        cout << "\nАктивні бронювання читача "
            << readers[readerIndex].name << ":\n";

        bool any = false;

        for (const Reservation& reservation : reservations) {
            if (reservation.readerId != readerId)
                continue;

            int bookIndex = findBook(reservation.bookId);

            if (bookIndex < 0)
                continue;

            cout << "- \"" << books[bookIndex].title
                << "\", отримання "
                << dateFromDay(reservation.day) << "\n";

            any = true;
        }

        if (!any)
            cout << "Активних бронювань немає.\n";
    }

    void bookBook() {
        int readerId = readInt("Номер читача: ", 1, 9999);

        printBooks();

        int bookId = readInt("Номер книги: ", 1, 9999);

        string reason = checkBooking(readerId, bookId);

        if (!reason.empty()) {
            cout << reason << "\n";
            return;
        }

        while (true) {
            printDays(bookId);

            int day = readInt(
                "Оберіть номер дати (0 — скасувати): ",
                0, DAYS
            );

            if (day == 0) {
                cout << "Бронювання скасовано користувачем.\n";
                return;
            }

            if (!isDayFree(bookId, day)) {
                cout << "Книга недоступна на обрану дату. "
                    << "Оберіть іншу дату.\n";
                continue;
            }

            if (readInt(
                "Підтвердити бронювання на "
                + dateFromDay(day) + "? (1 — так, 0 — ні): ",
                0, 1
            ) == 1) {
                reservations.push_back({ readerId, bookId, day });

                int bookIndex = findBook(bookId);
                int readerIndex = findReader(readerId);

                // ВИПРАВЛЕННЯ БАГУ №1: зменшуємо кількість вільних примірників у каталозі
                if (bookIndex >= 0 && books[bookIndex].available > 0) {
                    books[bookIndex].available--;
                }

                cout << "Бронювання оформлено: \""
                    << books[bookIndex].title << "\" для "
                    << readers[readerIndex].name
                    << ", отримання " << dateFromDay(day) << ".\n";
            }
            else {
                cout << "Бронювання не підтверджено.\n";
            }

            return;
        }
    }

    void cancelBooking() {
        int readerId = readInt("Номер читача: ", 1, 9999);
        int bookId = readInt("Номер книги: ", 1, 9999);

        int index = findReservation(readerId, bookId);

        if (index < 0) {
            cout << "Помилка: такого бронювання не знайдено.\n";
            return;
        }

        int bookIndex = findBook(bookId);

        if (bookIndex >= 0) {
            cout << "Бронювання \"" << books[bookIndex].title
                << "\" скасовано.\n";

            // ВИПРАВЛЕННЯ БАГУ №2: збільшуємо кількість вільних примірників у каталозі
            books[bookIndex].available++;
        }

        reservations.erase(reservations.begin() + index);
    }

    void run() {
        while (true) {
            cout << "\n=== Модуль «Бронювання книг» ===\n"
                << "1. Забронювати книгу\n"
                << "2. Переглянути бронювання читача\n"
                << "3. Скасувати бронювання\n"
                << "0. Вихід\n";

            int choice = readInt("Оберіть дію: ", 0, 3);

            if (choice == 0)
                break;
            else if (choice == 1)
                bookBook();
            else if (choice == 2)
                printReservations(
                    readInt("Номер читача: ", 1, 9999)
                );
            else
                cancelBooking();
        }

        cout << "Роботу завершено.\n";
    }

} // namespace Booking

// ======================================================================
// ВИДАЧА ТА ПОВЕРНЕННЯ
// ВІДПОВІДАЛЬНА: Андруневчин Вероніка
// ======================================================================

namespace IssueReturn {

    const int LOAN_DAYS = 14;
    const int FINE_PER_DAY = 2;
    const int MAX_LOANS = 3;

    using Search::Book;
    using Search::books;
    using Booking::Reader;
    using Booking::readers;
    using Booking::findBook;
    using Booking::findReader;
    using Booking::readInt;

    struct Loan {
        int readerId;
        int bookId;
    };

    // Важливо: список розміщено поза run().
    // Тому він не очищається після виходу з модуля.
    vector<Loan> loans;

    int findLoan(int readerId, int bookId) {
        for (int i = 0; i < (int)loans.size(); i++) {
            if (loans[i].readerId == readerId &&
                loans[i].bookId == bookId)
                return i;
        }

        return -1;
    }

    int countLoans(int readerId) {
        int count = 0;

        for (const Loan& loan : loans) {
            if (loan.readerId == readerId)
                count++;
        }

        return count;
    }

    // ---------- Видача книги ----------

    string issueBook(int readerId, int bookId) {
        int readerIndex = findReader(readerId);
        int bookIndex = findBook(bookId);

        if (readerIndex < 0)
            return "Помилка: читача не знайдено.";

        if (bookIndex < 0)
            return "Помилка: книгу не знайдено.";

        if (readers[readerIndex].debt > 0) {
            return "Відмова: у читача є борг "
                + to_string(readers[readerIndex].debt) + " грн.";
        }

        if (countLoans(readerId) >= MAX_LOANS) {
            return "Відмова: читач уже тримає "
                + to_string(MAX_LOANS) + " книги.";
        }

        if (findLoan(readerId, bookId) >= 0)
            return "Відмова: цю книгу читач уже отримав.";

        if (books[bookIndex].available <= 0)
            return "Відмова: вільних примірників немає.";

        // ВИПРАВЛЕННЯ БАГУ №2 (початок): враховуємо бронювання інших читачів 
        int reservedByOthers = 0;

        for (const Booking::Reservation& res : Booking::reservations) {
            if (res.bookId == bookId && res.readerId != readerId)
                reservedByOthers++;
        }

        if (books[bookIndex].available - reservedByOthers <= 0)
            return "Відмова: книгу заброньовано іншим читачем.";
        // ВИПРАВЛЕННЯ БАГУ №2 (кінець)

        books[bookIndex].available--;

        loans.push_back({ readerId, bookId });

        // ВИПРАВЛЕННЯ БАГУ №1 (початок): бронювання виконано, знімаємо його
        int resIndex = Booking::findReservation(readerId, bookId);

        if (resIndex >= 0) {
            Booking::reservations.erase(
                Booking::reservations.begin() + resIndex
            );
        }
        // ВИПРАВЛЕННЯ БАГУ №1 (кінець)

        // Журнал містить ID, а не копії назв книг і читачів.
        LibraryData::operations.push_back({
            LibraryData::todayISO(),
            bookId,
            readerId,
            LibraryData::OperationType::Issue
            });

        return "Видано \"" + books[bookIndex].title
            + "\" читачу " + readers[readerIndex].name
            + ". Повернути протягом "
            + to_string(LOAN_DAYS) + " днів.";
    }

    // ---------- Повернення книги ----------

    string returnBook(int readerId, int bookId, int daysHeld) {
        int readerIndex = findReader(readerId);
        int bookIndex = findBook(bookId);

        if (readerIndex < 0)
            return "Помилка: читача не знайдено.";

        if (bookIndex < 0)
            return "Помилка: книгу не знайдено.";

        int loanIndex = findLoan(readerId, bookId);

        if (loanIndex < 0) {
            return "Помилка: запису про видачу цієї книги "
                "цьому читачу немає.";
        }

        int overdue = daysHeld > LOAN_DAYS
            ? daysHeld - LOAN_DAYS : 0;

        int fine = overdue * FINE_PER_DAY;

        readers[readerIndex].debt += fine;
        books[bookIndex].available++;

        loans.erase(loans.begin() + loanIndex);

        LibraryData::operations.push_back({
            LibraryData::todayISO(),
            bookId,
            readerId,
            LibraryData::OperationType::Return
            });

        string message = "Прийнято \"" + books[bookIndex].title
            + "\" від читача " + readers[readerIndex].name
            + ". Днів користування: " + to_string(daysHeld) + ".";

        if (overdue > 0) {
            message += " Прострочення: " + to_string(overdue)
                + " дн. Штраф: " + to_string(fine)
                + " грн (" + to_string(FINE_PER_DAY)
                + " грн/день). Загальний борг читача: "
                + to_string(readers[readerIndex].debt) + " грн.";
        }
        else {
            message += " Прострочення немає, штраф не нараховано.";
        }

        return message;
    }

    // ---------- Перелік книг на руках ----------

    void printLoans() {
        cout << "\n--- Книги на руках ---\n";

        if (loans.empty()) {
            cout << "Зараз жодної книги не видано.\n";
            return;
        }

        int number = 1;

        for (const Loan& loan : loans) {
            int readerIndex = findReader(loan.readerId);
            int bookIndex = findBook(loan.bookId);

            cout << number++ << ". Читач №" << loan.readerId
                << " "
                << (readerIndex >= 0
                    ? readers[readerIndex].name : "?")
                << " — книга №" << loan.bookId
                << " \""
                << (bookIndex >= 0
                    ? books[bookIndex].title : "?")
                << "\"\n";
        }
    }

    void printCatalog() {
        cout << "\n--- Книги (номер, назва, вільних примірників) ---\n";

        for (const Book& book : books) {
            cout << book.id << ". " << book.title
                << " — вільних: " << book.available << "\n";
        }

        cout << "\n--- Читачі (номер, ПІБ, борг) ---\n";

        for (const Reader& reader : readers) {
            cout << reader.id << ". " << reader.name
                << " — борг: " << reader.debt << " грн\n";
        }
    }

    void run() {
        cout << "Модуль «Обслуговування читачів "
            << "(видача та повернення)»\n";

        printCatalog();

        while (true) {
            cout << "\n===== МЕНЮ =====\n"
                << "1. Видача книги\n"
                << "2. Повернення книги\n"
                << "3. Переглянути книги на руках\n"
                << "4. Показати книги та читачів\n"
                << "0. Вихід\n";

            int choice = readInt("Оберіть операцію: ", 0, 4);

            if (choice == 0) {
                cout << "Роботу завершено.\n";
                break;
            }

            if (choice == 3) {
                printLoans();
                continue;
            }

            if (choice == 4) {
                printCatalog();
                continue;
            }

            int readerId = readInt("Введіть номер читача: ", 1, 9999);
            int bookId = readInt("Введіть номер книги: ", 1, 9999);

            if (choice == 1) {
                cout << issueBook(readerId, bookId) << "\n";
            }
            else {
                int days = readInt(
                    "Скільки днів читач користувався книгою: ",
                    0, 3650
                );

                cout << returnBook(readerId, bookId, days) << "\n";
            }
        }
    }

} // namespace IssueReturn


// ======================================================================
// СТАТИСТИКА
// ВІДПОВІДАЛЬНА: Гразьон Поліна
// ======================================================================

namespace Statistics {

    using Booking::readInt;
    using LibraryData::Operation;
    using LibraryData::OperationType;

    struct StatisticsInput {
        string startDate;
        string endDate;

        // Порожній рядок означає «усі категорії».
        string readerCategory;
        string bookCategory;
    };

    struct StatisticsResult {
        int totalIssued = 0;
        int totalReturned = 0;
        int periodDays = 0;

        double averageDailyTurnover = 0;

        string mostPopularBook = "Немає даних";
        string mostActiveReaderCategory = "Немає даних";

        map<string, int> bookCounts;
        map<string, int> readerCounts;
        map<int, int> weeklyTurnover;
    };

    // ---------- Робота з датами ----------

    int daysInMonth(int month, int year) {
        if (month == 2) {
            bool leapYear =
                (year % 400 == 0) ||
                (year % 4 == 0 && year % 100 != 0);

            return leapYear ? 29 : 28;
        }

        if (month == 4 || month == 6 ||
            month == 9 || month == 11) {
            return 30;
        }

        return 31;
    }

    bool isValidDate(string date) {
        if (date.length() != 10 ||
            date[4] != '-' || date[7] != '-') {
            return false;
        }

        for (int i = 0; i < 10; i++) {
            if (i != 4 && i != 7 &&
                (date[i] < '0' || date[i] > '9')) {
                return false;
            }
        }

        int year = stoi(date.substr(0, 4));
        int month = stoi(date.substr(5, 2));
        int day = stoi(date.substr(8, 2));

        if (year < 1 || month < 1 || month > 12)
            return false;

        return day >= 1 && day <= daysInMonth(month, year);
    }

    int dateToSerial(string date) {
        int year = stoi(date.substr(0, 4));
        int month = stoi(date.substr(5, 2));
        int day = stoi(date.substr(8, 2));

        int totalDays = day;

        for (int y = 1; y < year; y++) {
            bool leapYear =
                (y % 400 == 0) ||
                (y % 4 == 0 && y % 100 != 0);

            totalDays += leapYear ? 366 : 365;
        }

        for (int m = 1; m < month; m++)
            totalDays += daysInMonth(m, year);

        return totalDays;
    }

    string makeDate(int year, int month, int day) {
        string date = to_string(year) + "-";

        if (month < 10)
            date += "0";

        date += to_string(month) + "-";

        if (day < 10)
            date += "0";

        date += to_string(day);

        return date;
    }

    // ---------- Категорії з основних структур ----------

    // Формує список унікальних категорій на основі каталогу.
    vector<string> getBookCategories() {
        vector<string> categories;

        for (const Search::Book& book : Search::books) {
            bool exists = false;

            for (const string& category : categories) {
                if (category == book.category) {
                    exists = true;
                    break;
                }
            }

            if (!exists)
                categories.push_back(book.category);
        }

        return categories;
    }

    // Формує список унікальних категорій на основі читачів.
    vector<string> getReaderCategories() {
        vector<string> categories;

        for (const Booking::Reader& reader : Booking::readers) {
            bool exists = false;

            for (const string& category : categories) {
                if (category == reader.category) {
                    exists = true;
                    break;
                }
            }

            if (!exists)
                categories.push_back(reader.category);
        }

        return categories;
    }

    // ---------- Демонстраційні операції ----------

    // Створює демонстраційну історію через ID наявних книг і читачів.
    // Назви книг та категорії тут не копіюються.
    void createDemoRecords() {
        static bool initialized = false;

        if (initialized)
            return;

        initialized = true;

        if (Search::books.empty() || Booking::readers.empty())
            return;

        // Генеруємо демонстраційні операції за 2026 рік.
        for (int month = 1; month <= 12; month++) {
            for (int day = 1;
                day <= daysInMonth(month, 2026);
                day++) {

                string date = makeDate(2026, month, day);

                // Вісім демонстраційних видач на день.
                for (int n = 0; n < 8; n++) {
                    int bookIndex =
                        (day * 3 + month * 2 + n * 4)
                        % Search::books.size();

                    int readerIndex =
                        (day + n) % Booking::readers.size();

                    int bookId = Search::books[bookIndex].id;
                    int readerId = Booking::readers[readerIndex].id;

                    LibraryData::operations.push_back({
                        date,
                        bookId,
                        readerId,
                        OperationType::Issue
                        });

                    // Частина книг має демонстраційну операцію повернення.
                    if ((day + month + n) % 3 != 0) {
                        LibraryData::operations.push_back({
                            date,
                            bookId,
                            readerId,
                            OperationType::Return
                            });
                    }
                }
            }
        }
    }

    // ---------- Введення параметрів ----------

    void inputParams(StatisticsInput& input) {
        while (true) {
            cout << "\nВведіть початкову дату (YYYY-MM-DD): ";
            cin >> input.startDate;

            cout << "Введіть кінцеву дату (YYYY-MM-DD): ";
            cin >> input.endDate;

            if (!isValidDate(input.startDate) ||
                !isValidDate(input.endDate)) {
                cout << "Помилка: введено неправильну дату.\n";
                continue;
            }

            if (dateToSerial(input.startDate) >
                dateToSerial(input.endDate)) {
                cout << "Помилка: початкова дата пізніша за кінцеву.\n";
                continue;
            }

            break;
        }

        vector<string> readerCategories = getReaderCategories();
        vector<string> bookCategories = getBookCategories();

        cout << "\nОберіть категорію читачів:\n";

        for (int i = 0; i < (int)readerCategories.size(); i++)
            cout << i + 1 << " — " << readerCategories[i] << "\n";

        cout << readerCategories.size() + 1
            << " — Усі категорії\n";

        int readerChoice = readInt(
            "Ваш вибір: ",
            1,
            (int)readerCategories.size() + 1
        );

        input.readerCategory = "";

        if (readerChoice <= (int)readerCategories.size())
            input.readerCategory = readerCategories[readerChoice - 1];

        cout << "\nОберіть категорію книг:\n";

        for (int i = 0; i < (int)bookCategories.size(); i++)
            cout << i + 1 << " — " << bookCategories[i] << "\n";

        cout << bookCategories.size() + 1
            << " — Усі категорії\n";

        int bookChoice = readInt(
            "Ваш вибір: ",
            1,
            (int)bookCategories.size() + 1
        );

        input.bookCategory = "";

        if (bookChoice <= (int)bookCategories.size())
            input.bookCategory = bookCategories[bookChoice - 1];
    }

    // ---------- Обробка статистики ----------

    StatisticsResult processStatistics(
        const vector<Operation>& operations,
        const StatisticsInput& input
    ) {
        StatisticsResult result;

        int start = dateToSerial(input.startDate);
        int end = dateToSerial(input.endDate);

        result.periodDays = end - start + 1;

        for (const Operation& operation : operations) {
            if (!isValidDate(operation.date))
                continue;

            int operationDate = dateToSerial(operation.date);

            if (operationDate < start || operationDate > end)
                continue;

            // Знаходимо книгу й читача за ID в основних структурах.
            int bookIndex = Search::findBook(operation.bookId);
            int readerIndex = Booking::findReader(operation.readerId);

            if (bookIndex < 0 || readerIndex < 0)
                continue;

            const Search::Book& book = Search::books[bookIndex];
            const Booking::Reader& reader =
                Booking::readers[readerIndex];

            // Фільтри використовують категорії з основних структур.
            if (!input.bookCategory.empty() &&
                book.category != input.bookCategory) {
                continue;
            }

            if (!input.readerCategory.empty() &&
                reader.category != input.readerCategory) {
                continue;
            }

            if (operation.type == OperationType::Issue) {
                result.totalIssued++;

                result.bookCounts[book.title]++;
                result.readerCounts[reader.category]++;

                int week = (operationDate - start) / 7 + 1;
                result.weeklyTurnover[week]++;
            }
            else if (operation.type == OperationType::Return) {
                result.totalReturned++;
            }
        }

        if (result.totalIssued > 0) {
            result.averageDailyTurnover =
                static_cast<double>(result.totalIssued)
                / result.periodDays;
        }

        // Найпопулярніша книга.
        int best = 0;

        for (const auto& item : result.bookCounts) {
            if (item.second > best) {
                best = item.second;
                result.mostPopularBook = item.first;
            }
        }

        // Найактивніша категорія читачів.
        best = 0;

        for (const auto& item : result.readerCounts) {
            if (item.second > best) {
                best = item.second;
                result.mostActiveReaderCategory = item.first;
            }
        }

        return result;
    }

    // ---------- Виведення результатів ----------

    void printStatistics(
        const StatisticsResult& result,
        const StatisticsInput& input
    ) {
        cout << "\n===== СТАТИСТИКА ЗА ПЕРІОД "
            << input.startDate << " — " << input.endDate
            << " =====\n";

        cout << "Кількість днів у періоді: "
            << result.periodDays << "\n";

        cout << "Видано книг: "
            << result.totalIssued << "\n";

        cout << "Повернуто книг: "
            << result.totalReturned << "\n";

        cout << "Середньодобовий обіг: "
            << fixed << setprecision(2)
            << result.averageDailyTurnover << "\n";

        cout << "Найпопулярніша книга: "
            << result.mostPopularBook << "\n";

        cout << "Найактивніша категорія читачів: "
            << result.mostActiveReaderCategory << "\n";

        cout << "\nОбіг по тижнях:\n";

        if (result.weeklyTurnover.empty())
            cout << "Немає даних.\n";

        for (const auto& week : result.weeklyTurnover) {
            cout << "Тиждень " << setw(2) << week.first
                << ": " << week.second << "\n";
        }
    }

    void run() {
        createDemoRecords();

        while (true) {
            StatisticsInput input;

            inputParams(input);

            StatisticsResult result = processStatistics(
                LibraryData::operations,
                input
            );

            printStatistics(result, input);

            if (readInt(
                "\nПовторити аналіз? (1 — так, 0 — назад до меню): ",
                0, 1
            ) != 1) {
                break;
            }
        }
    }

} // namespace Statistics


// ======================================================================
// ГОЛОВНЕ МЕНЮ
// ======================================================================

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    // Демонстраційна історія створюється один раз на запуск.
    Statistics::createDemoRecords();

    while (true) {
        cout << "\n===== АС «Бібліотека ЗВО» =====\n"
            << "1. Пошук книг\n"
            << "2. Бронювання книг\n"
            << "3. Видача та повернення\n"
            << "4. Статистика\n"
            << "0. Вихід\n";

        int choice = Booking::readInt("Оберіть модуль: ", 0, 4);

        if (choice == 0)
            break;

        if (choice == 1)
            Search::run();
        else if (choice == 2)
            Booking::run();
        else if (choice == 3)
            IssueReturn::run();
        else
            Statistics::run();
    }

    cout << "Роботу завершено.\n";

    return 0;
}