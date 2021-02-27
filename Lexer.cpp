#include "Lexer.h"
#include <iomanip>
#include <iostream>
#include <string>
#include <sstream>

using namespace std;

namespace Parse {
	char Reader::get() {
		if (input.peek() == '\n') {
			line_++;
			col_ = 1;
		}
		else if (input.peek() == '\t') col_ += 4;
		else col_++;
		return input.get();
	}

	void Reader::ignore() {
		get();
	}

	char Reader::peek() {
		return input.peek();
	}

	size_t Reader::line() const { return line_; }

	size_t Reader::col() const { return col_; }

	size_t Reader::size() {
		input.seekg(0, ios::end);
		size_t size = static_cast<size_t>(input.tellg());
		input.seekg(0);
		return size;
	}

	Position Reader::position() const { return { line_, col_ }; }

	void Lexer::Whitespace() {
		char next;
		while (!gramar->symbols_attributes[next = program.peek()]) {
			if (next == '\n') row++;
			program.ignore();
			if (program.peek() == Eof) break;
		}
	}

	void Lexer::Comment() {
		program.ignore();
		if (program.peek() != '*') ThrowErr("Unopened comment");
		bool met_an_asterisk = false;
		char next;
		while (program) {
			next = program.get();
			if (met_an_asterisk && next == ')')
				return;
			met_an_asterisk = (next == '*');
		}
		ThrowErr("Unclosed comment");
	}

	void Lexer::Delimiter() {
		auto begin = program.position();
		size_t pos = Position();
		Program() += program.get();
		Tokens().emplace_back(static_cast<Code>(Program().back()), begin, string_view{ &Program().c_str()[pos], 1 });
	}

	void Lexer::KeywordOrIdentifier() {
		auto begin = program.position();
		string buffer;
		char next;
		while (program) {
			next = program.peek();
			if (isalnum(next)) buffer += program.get();
			else break;
		}
		size_t pos = Position();
		Program() += buffer;
		if (gramar->key_words.count(buffer)) 
			Tokens().emplace_back(gramar->key_words[buffer], begin, string_view{ &Program().c_str()[pos], buffer.size() });
		else {
			gramar->identifiers.try_emplace(string_view{ &Program().c_str()[pos], buffer.size() },
				gramar->identifier_code + gramar->identifiers.size());
			Tokens().emplace_back(gramar->identifiers[buffer], begin, string_view{ &Program().c_str()[pos], buffer.size() });
		}
	}

	size_t Lexer::Position() const { return GetProgram().size(); }

	void Lexer::LeftPart(string& buffer) {
		while (program) {
			if (isdigit(program.peek())) {
				buffer += program.peek();
				program.get();
			}
			else {
				char next = program.peek();
				if (!gramar->symbols_attributes[next] || next == '\'') return;
				ThrowErr("Wrong left part");
			}
		}
	}

	void Lexer::RightPart(string& buffer) {
		if (isdigit(program.peek())) {
			LeftPart(buffer);
			return;
		}
		string local_buffer;
		while (program) {
			if (program.peek() == '(') {
				program.ignore();
				if (local_buffer != "$EXP") 
					ThrowErr("Wrong right part : unknown word " + local_buffer);
				local_buffer += '(';
				break;
			}
			local_buffer += program.get();
		}
		Whitespace();
		while (program) {
			if (isdigit(program.peek()))
				local_buffer += program.get();
			else {
				Whitespace();
				if (program.peek() == ')') {
					buffer += local_buffer + ')';
					program.ignore();
					return;
				}
				ThrowErr("Wrong right part : unclosed exponent body");
			}
		}
	}

	void Lexer::AddErr(std::string&& msg) {
		Errors().emplace_back(msg);
	}

	void Lexer::ThrowErr(std::string&& msg) {
		throw LexerError{ "Lexer: Error (line " + to_string(program.line()) +
			", col " + to_string(program.col() - 1) + ") : " + msg + ";" };
	}

	void Lexer::Constant() {
		auto begin = program.position();
		string buffer;
		program.get();
		bool error = 0;
		Whitespace();
		try {
			LeftPart(buffer);
		}
		catch (LexerError& ex) {
			error = true;
			AddErr(ex.what());
		}
		Whitespace();
		if (program.peek() != '\'' && !error) {
			try {
				RightPart(buffer);
			}
			catch (LexerError& ex) {
				error = true;
				AddErr(ex.what());
			}
		}
		while (program && program.get() != '\'');
		if (!program) ThrowErr("Unclosed constant");
		if (error) return;
		size_t pos = Position();
		Program() += '\'' + buffer + '\'';
		gramar->constants.try_emplace(string_view{ &Program().c_str()[pos+1], buffer.size() },
			gramar->constant_code + gramar->constants.size());
		Tokens().emplace_back(gramar->constants[buffer], begin, string_view{ &Program().c_str()[pos], buffer.size()+2 });
	}

	void Lexer::Parse() {
		if (parsed_program.has_value()) return;
		parsed_program.emplace();
		Program().reserve(program.size() + 1);
		while(program.peek() != Eof) {
			char next = program.peek();
			try {
				switch (gramar->symbols_attributes[next]) {
				case 0:
					Whitespace();
					break;
				case 2:
					KeywordOrIdentifier();
					break;
				case 3:
					Delimiter();
					break;
				case 4:
					Comment();
					break;
				case 6:
					Constant();
					break;
				default:
					program.ignore();
					stringstream out;
					out << "Illegal symbol \'" << next << "\'";
					ThrowErr(out.str());
				}
			}
			catch (LexerError& ex) {
				AddErr(ex.what());
			}
		}
	}

	const Lexer::LexemesList &Lexer::List() const {
		if (parsed_program.has_value())
			return parsed_program.value();
		throw bad_optional_access();
	}

	const std::vector<std::string>& Lexer::GetErrors() const { return List().errors; }

	const std::vector<Lexer::LexemesList::Item>& Lexer::GetTokens() const { return List().items; }

	const std::string& Lexer::GetProgram() const { return List().program; }

	Lexer::LexemesList& Lexer::List() {
		if (parsed_program.has_value())
			return parsed_program.value();
		throw bad_optional_access();
	}

	std::vector<std::string>& Lexer::Errors() { return List().errors; }

	std::vector<Lexer::LexemesList::Item>& Lexer::Tokens() { return List().items; }

	std::string& Lexer::Program() { return List().program; }

	bool operator==(const Lexer::LexemesList::Item& lhs, const Lexer::LexemesList::Item& rhs) {
		if (lhs.code != rhs.code) return false;
		if (lhs.position != rhs.position) return false;
		return true;
	}

	bool operator==(const Lexer::LexemesList& lhs, const Lexer::LexemesList& rhs) {
		if (lhs.items != rhs.items) return false;
		if (lhs.program != rhs.program) return false;
		return true;
	}

	std::ostream& operator<<(std::ostream& os, const Lexer::LexemesList& list) {
		os << endl;
		for (const auto& item : list.items)
			os << setw(5) << item.code;
		os << endl;
		for (const auto& item : list.items)
			os << setw(5) << item.position;
		return os;
	}

	std::ostream& operator<<(std::ostream& os, const Lexer::LexemesList::Item& item) {
		os << item.code << " on position " << item.position;
		return os;
	}

	bool operator== (const Position& lhs, const Position& rhs) {
		return tie(lhs.line, lhs.col) == tie(rhs.line, rhs.col);
	}

	bool operator!= (const Position& lhs, const Position& rhs) {
		return !(lhs == rhs);
	}

	ostream& operator<< (ostream& os, const Position& elem) {
		os << "[" << elem.line << "; " << elem.col << "]";
		return os;
	}
}