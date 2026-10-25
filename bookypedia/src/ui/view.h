#pragma once
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>
#include "../postgres/postgres.h"

namespace menu { class Menu; }

namespace ui {
class View {
public:
    View(menu::Menu& menu, postgres::Database& db, std::istream& input, std::ostream& output);
private:
    bool AddAuthor(std::istream&) const;
    bool ShowAuthors() const;
    bool AddBook(std::istream&) const;
    bool ShowBooks() const;
    bool ShowAuthorBooks() const;
    bool DeleteAuthor(std::istream&) const;
    bool EditAuthor(std::istream&) const;
    bool ShowBook(std::istream&) const;
    bool DeleteBook(std::istream&) const;
    bool EditBook(std::istream&) const;

    std::optional<domain::Author> SelectAuthor(bool print_select) const;
    std::optional<domain::Author> ResolveAuthor(std::string name, bool print_select) const;
    std::optional<postgres::BookInfo> SelectBook(std::string title) const;
    std::vector<std::string> ReadTags() const;

    void PrintAuthors(const std::vector<domain::Author>& authors) const;
    void PrintBooks(const std::vector<postgres::BookInfo>& books) const;
    void PrintBook(const postgres::BookInfo& book) const;

    menu::Menu& menu_;
    postgres::Database& db_;
    std::istream& input_;
    std::ostream& output_;
};
}  // namespace ui
