#ifndef M_DOCUMNT
#define M_DOCUMENT

#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace Document {

struct Document {
private:
    std::vector<std::string> data;

public:
    Document();

    // Document info
    const std::vector<std::string> get_data() const;
    size_t lines_count();
    size_t line_length(size_t line);

    // Document modification
    void insert_line(size_t line, const std::string& content);
    void delete_line(size_t line);
    void break_line(size_t line, size_t column);
    void insert_char(size_t line, size_t column, char ch);
    void delete_char(size_t line, size_t column);
};

struct Cursor {
    size_t line, column;
    Document* document;
    void set(size_t _line, size_t _column);
    void set(const Cursor& other);

    Cursor();
    Cursor(Document* document);
    Cursor(Document* document, const Cursor& other);
    Cursor(Document* document, size_t _line, size_t _column);

    void sync_with_document();
    void home();
    void end();
    void up();
    void down();
    void left();
    void right();
    void write(char ch);
    void del();
    void backspace();
    void break_line();
    void tab();
};

struct Cursor_image {
    Cursor_image();
    Cursor_image(size_t line, size_t column, size_t id);

    bool operator<(const Cursor_image& other);

    size_t line, column, id;
};

struct Document_image {
    Document_image();
    // Regular constructor, serializes the object
    Document_image(const std::vector<std::string>& data,
        const std::vector<Cursor_image>& cursors);
    // Constructor from string, deserializes the object
    Document_image(const std::string& serialized_object);

    // Je prakticke aby cursory boli usortene
    void sort_cursors();

    std::vector<std::string> data;
    std::vector<Cursor_image> cursors;
    std::string serialized_object;
};

struct Document_handler {
private:
    Document document;
    std::map<int, Cursor> cursors;
    std::mutex mtx;

public:
    Document_handler();

    // API
    bool process_message(int cursor_id, std::string message);

    // Cursor handling
    void add_new_cursor(int cursor_id);
    void remove_cursor(int cursor_id);
    Cursor* get_cursor(int cursor_id);

    // Serialization
    Document_image get_document_image() const;
    std::string
    serialize() const; // toto je dost useless, asi to neskor vymazem

    // Dev features
    void print();
};

}

#endif