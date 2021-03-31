#include <algorithm>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#include "document.h"

namespace Document {

Document::Document()
    : data(std::vector<std::string>(1))
{
}

size_t Document::lines_count() { return data.size(); }

size_t Document::line_length(size_t line)
{
    if (line >= lines_count())
        return 0;
    else
        return data[line].length();
}

void Document::insert_line(size_t line, const std::string& content = "")
{
    if (line > lines_count())
        line = lines_count();

    data.insert(data.begin() + line, content);
}

void Document::delete_line(size_t line)
{
    if (line < lines_count())
        data.erase(data.begin() + line);
}

void Document::break_line(size_t line, size_t column)
{
    if (line < lines_count()) {
        std::string new_line = data[line].substr(column);
        data.insert(data.begin() + line + 1, new_line);
        data[line] = data[line].substr(0, column);
    }
}

void Document::insert_char(size_t line, size_t column, char ch)
{
    if (line < lines_count()) {
        data[line].insert(data[line].begin() + column, 1, ch);
    }
}

void Document::delete_char(size_t line, size_t column)
{
    if (line < lines_count()) {
        // line musi byt v rangi
        if ((column == line_length(line)) and (line + 1 < lines_count())) {
            // ak mazeme na konci riadku, musime vymazat line break
            std::string current_line_content = data[line + 1];
            data.erase(data.begin() + line + 1);
            data[line] += current_line_content;

        } else if (column < line_length(line))
            data[line].erase(column, 1);
    }
}

void Cursor::set(size_t _line, size_t _column)
{
    line = _line;
    column = _column;
}
void Cursor::set(const Cursor& other) { set(other.line, other.column); }

Cursor::Cursor()
    : line(0)
    , column(0)
    , document(nullptr)
{
}

Cursor::Cursor(Document* document)
    : line(0)
    , column(0)
    , document(document)
{
}

Cursor::Cursor(Document* document, size_t _line, size_t _column)
    : line(_line)
    , column(_column)
    , document(document)
{
}
Cursor::Cursor(Document* document, const Cursor& other)
    : line(other.line)
    , column(other.column)
    , document(document)
{
}

void Cursor::sync_with_document()
{
    if (line >= document->lines_count())
        return set(document->lines_count() - 1,
            document->line_length(document->lines_count() - 1));
    else {
        size_t new_column = std::min(column, document->line_length(line));
        set(line, new_column);
    }
}

void Cursor::home()
{
    sync_with_document();
    set(line, 0);
}

void Cursor::end()
{
    sync_with_document();
    set(line, document->line_length(line));
}

void Cursor::up()
{
    sync_with_document();
    if (line == 0)
        home();
    else {
        set(line - 1, column);
        sync_with_document();
    }
}

void Cursor::down()
{
    sync_with_document();
    set(line + 1, column);
    sync_with_document();
}

void Cursor::left()
{
    sync_with_document();
    if (column == 0) {
        if (line != 0) {
            up();
            end();
        }
    } else
        set(line, column - 1);
}

void Cursor::right()
{
    sync_with_document();
    if (column == document->line_length(line)) {
        if (line != document->lines_count() - 1) {
            down();
            home();
        }
    } else
        set(line, column + 1);
}

void Cursor::write(char ch)
{
    sync_with_document();
    document->insert_char(line, column, ch);
    right();
}

void Cursor::del()
{
    // TODO: delete na konci riadku nemaze newline
    sync_with_document();
    document->delete_char(line, column);
}

void Cursor::backspace()
{
    if ((line != 0) or (column != 0)) {
        sync_with_document();
        left();
        del();
    }
}

void Cursor::break_line()
{
    sync_with_document();
    document->break_line(line, column);
    down();
    home();
}

void Cursor::tab()
{
    size_t amount_of_spaces = ((column + 4) / 4) * 4 - column;
    for (size_t i = 0; i < amount_of_spaces; ++i) {
        write(' ');
    }
}

Cursor_image::Cursor_image() { }

Cursor_image::Cursor_image(size_t line, size_t column, size_t id)
    : line(line)
    , column(column)
    , id(id)
{
}

bool Cursor_image::operator<(const Cursor_image& other)
{
    if (line == other.line)
        return column < other.column;
    return line < other.line;
}

Document_image::Document_image() { }

// Regular constructor, serializes the object
Document_image::Document_image(const std::vector<std::string>& data,
    const std::vector<Cursor_image>& cursors)
    : data(data)
    , cursors(cursors)
{
    // aby sme mohli garantovat usortenie cursorov
    sort_cursors();

    std::stringstream ss;
    ss << cursors.size() << "\n";
    for (auto&& c : cursors)
        ss << c.line << " " << c.column << " " << c.id << "\n";
    ss << data.size() << "\n";
    for (auto&& line : data)
        ss << line << "\n";
    serialized_object = ss.str();
}
// Constructor from string, deserializes the object
Document_image::Document_image(const std::string& serialized_object)
    : serialized_object(serialized_object)
{
    std::stringstream ss(serialized_object);
    size_t cursors_count;
    ss >> cursors_count;
    cursors.resize(cursors_count);
    for (size_t i = 0; i < cursors_count; ++i) {
        size_t line, column, id;
        ss >> line >> column >> id;
        cursors[i] = Cursor_image(line, column, id);
    }
    size_t data_size;
    ss >> data_size;
    data.resize(data_size);

    // Treba nacitat koniec tohoto riadka, dalsie veci treba citat getlinom, ale
    //  este stale som na riadku s poctom riadkov
    std::string dummy_variable;
    getline(ss, dummy_variable);

    for (size_t i = 0; i < data_size; ++i)
        getline(ss, data[i]);

    // aby sme mohli garantovat usortenie cursorov
    sort_cursors();
}

void Document_image::sort_cursors()
{
    std::sort(cursors.begin(), cursors.end());
}

Document_handler::Document_handler() { }

bool Document_handler::process_message(int cursor_id, std::string message)
{
    Cursor* cursor = get_cursor(cursor_id);
    if (cursor == nullptr)
        return false;

    char first_char = message[0], second_char = message[1];
    switch (first_char) {
    case 'W':
        cursor->write(second_char);
        break;
    case 'S':
        switch (second_char) {
        case 'U':
            cursor->up();
            break;
        case 'D':
            cursor->down();
            break;
        case 'R':
            cursor->right();
            break;
        case 'L':
            cursor->left();
            break;
        case 'H':
            cursor->home();
            break;
        case 'E':
            cursor->end();
            break;
        case 'B':
            cursor->break_line();
            break;
        case 'X':
            cursor->del();
            break;
        case 'A':
            cursor->backspace();
            break;
        case 'T':
            cursor->tab();
            break;
        default:
            std::cerr << "Unknown message.\n";
            break;
        }
        break;
    default:
        std::cerr << "Unknown message.\n";
        return false;
        break;
    }

    return true;
}

void Document_handler::add_new_cursor(int cursor_id)
{
    if (cursors.find(cursor_id) != cursors.end())
        std::cerr << "Cursor with id " << cursor_id << " already exists.\n";
    else {
        cursors[cursor_id] = Cursor(&document);
    }
}

void Document_handler::remove_cursor(int cursor_id)
{
    cursors.erase(cursor_id);
}

Cursor* Document_handler::get_cursor(int cursor_id)
{
    if (cursors.find(cursor_id) == cursors.end()) {
        std::cerr << "Cursor with such id does not exist.\n";
        return nullptr;
    } else
        return std::addressof(cursors.at(cursor_id));
}

Document_image Document_handler::get_document_image() const
{
    std::vector<Cursor_image> cursor_images(cursors.size());
    size_t i = 0;
    for (auto&& cp : cursors) {
        cursor_images[i]
            = Cursor_image(cp.second.line, cp.second.column, cp.first);
        ++i;
    }

    return Document_image(document.data, cursor_images);
}

std::string Document_handler::serialize() const
{
    return get_document_image().serialized_object;
}

void Document_handler::print()
{
    std::cout << "--------------------------------\n"
              << serialize() << "--------------------------------\n";
}
}
