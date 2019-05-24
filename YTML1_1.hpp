#include <DirectXMath.h>
#include <memory>
#include <vector>
#include <functional>
#include <iostream>
#include <unordered_map>
#include <fstream>
#include <string>
#include <string_view>
#include <charconv>

extern void OutputDebugStringA(const char* lpOutputString);

inline namespace YTML1_1
{
	enum ElementFlag {
		RatioSizeWidth = 0b1,
		RatioSizeHeight = 0b10,
		RatioHorizontalAlign = 0b100,
		RatioVerticalAlign = 0b1000,
		Enable = 0b10000,
	};

	enum class ElementHorizontalAlign {
		Left, Center, Right
	};
	enum class ElementVerticalAlign {
		Top, Middle, Bottom
	};
	enum class ElementParentClipDirection {
		Horizontal, Vertical
	};

	struct FloatSize {
		float w = 0.f;
		float h = 0.f;
	};

	struct FloatRect {
		float x = 0.f;
		float y = 0.f;
		float w = 0.f;
		float h = 0.f;
	};
	
	inline void SplitByBlank(std::vector<std::string_view>& sv, const std::string& s)
	{
		size_t start = -1;
		for (size_t i = 0; i < s.size(); ++i)
		{
			if (s.at(i) == ' ')
			{
				if (start != -1)
				{
					sv.push_back(std::string_view(&s.at(start), i - start));
					//std::cout << s.substr(start, i - start) << std::endl;
					start = -1;
				}
			}
			else if (start == -1) start = i;
		}
		if (start != -1)
		{
			sv.push_back(std::string_view(&s.at(start), s.size() - start));
			//std::cout << s.substr(start, i - start) << std::endl;
			start = -1;
		}
	}
	union FourDirection {
		DirectX::XMFLOAT4 flt4;
		struct {
			float left;
			float top;
			float right;
			float bottom;
		};
	};

	extern struct Element;

	
	inline void ReadCSS(const std::string& path, std::unordered_map<std::string, std::string>& style)
	{
		std::ifstream file(path);

		std::string str((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

		file.close();

		size_t size = 0, i = 0, j = 0, k, meta_start;
		std::string obj;
		for (i = 0; i < str.size(); ++i) if (const char c = str.at(i); c != '\r' && c != '\n' && c != '\t') ++size;
		obj.resize(size);
		for (i = 0; i < str.size(); ++i) if (const char c = str.at(i); c != '\r' && c != '\n' && c != '\t') obj.at(j++) = c;



		//std::cout << obj << std::endl;

		size_t start = 0;
		std::vector<std::string_view> sv;
		for (i = 0; i < obj.size(); ++i)
		{
			if (obj.at(i) == '}')
			{
				std::string_view s(&obj.at(start), i - start);
				for (j = 0; j < s.size(); ++j)
				{
					if (s.at(j) == '{')
					{
						std::string_view meta(&s.at(0), j);
						std::string_view data(&s.at(j + 1), s.size() - j - 1);

						sv.clear();

						meta_start = -1;
						for (k = 0; k < meta.size(); ++k)
						{
							if (meta.at(k) == ',')
							{
								if (meta_start != -1)
								{
									sv.push_back(std::string_view(&meta.at(meta_start), k - meta_start));
									meta_start = -1;
								}
							}
							else if (meta_start == -1) meta_start = k;
						}
						if (meta_start != -1)
						{
							sv.push_back(std::string_view(&meta.at(meta_start), meta.size() - meta_start));
							meta_start = -1;
						}

						for (const auto& hs : sv)
						{
							size_t x, y;
							//Both Trip
							for (x = 0; x < hs.size(); ++x)
							{
								if (hs[x] != ' ') break;
							}
							for (y = hs.size(); y > 1; --y)
							{
								if (hs[y - 1] != ' ') break;
							}
							style[std::string(hs.substr(x, y - x))] = data;
							//std::cout << hs.substr(x, y - x) << ", ";
						}

						//std::cout << "~" << data << std::endl;
						break;
					}
				}
				//std::cout << s << std::endl;
				start = i + 1;
			}
		}
	}

	struct Element {
		FourDirection margin, border;

		FloatSize size = { 0.f, 0.f };
		ElementHorizontalAlign halign = ElementHorizontalAlign::Left;
		ElementVerticalAlign valign = ElementVerticalAlign::Top;
		ElementParentClipDirection pclip = ElementParentClipDirection::Horizontal;
		FloatRect size_in_display;

		size_t eid = -1;
		uint16_t flags = 16;

		std::string head;
		std::unordered_map<std::string, std::string> tuple;
		DirectX::XMFLOAT4 background_color = {1.f, 1.f, 1.f, 1.f};
		DirectX::XMFLOAT4 border_color = { 0.f, 0.f, 0.f, 1.f };
		
		Element() {
			margin.flt4 = { 0.f, 0.f, 0.f, 0.f };
			border.flt4 = { 0.f, 0.f, 0.f, 0.f };
		}

		void tupleChanged(const std::string& key, std::unordered_map<std::string, std::string>& style) {
			std::cout << "{" << key << ":" << tuple[key]  << "}" << std::endl;
			const auto& value = tuple[key];
			bool isWidth = key == "width";
			bool isHeight = key == "height";
			if (isWidth || isHeight)
			{
				size_t i, j, len;
				len = key.size();
				//Both Trip
				for (i = 0; i < value.size(); ++i)
				{
					if (value[i] != ' ') break;
				}
				for (j = value.size(); j > 1; --j)
				{
					if (value[j - 1] != ' ') break;
				}

				//Parse suffix %, px
				if (value[j - 2] == 'p' && value[j - 1] == 'x')
				{
					//std::cout << "|" << value.substr(i, j - i - 1) << "~px" << std::endl;
					if (isWidth)
					{
						std::from_chars(&value.at(i), &value.at(j - 1), size.w);
						//std::cout << value.substr(i, j - i - 1) << " = " << size.w << std::endl;
						//this->size.w = std::stof(std::string_view(&value.at(i), j - i - 1));
						flags &= ~(uint64_t)ElementFlag::RatioSizeWidth;
					}
					else if (isHeight)
					{
						std::from_chars(&value.at(i), &value.at(j - 1), size.h);
						//std::cout << value.substr(i, j - i - 1) << " = " << size.h << std::endl;
						flags &= ~(uint64_t)ElementFlag::RatioSizeHeight;
					}
				}
				else if (value[len - 1] == '%')
				{
					//std::cout << "|" << value.substr(i, j - i) << "~%" << std::endl;
					if (isWidth)
					{
						std::from_chars(&value.at(i), &value.at(j - 1), this->size.w);
						this->size.w /= 100.f;
						this->flags |= (uint64_t)ElementFlag::RatioSizeWidth;
					}
					else if (isHeight)
					{
						std::from_chars(&value.at(i), &value.at(j - 1), this->size.h);
						this->size.h /= 100.f;
						this->flags |= (uint64_t)ElementFlag::RatioSizeHeight;
					}
				}

				//std::cout << "|" << value.substr(i, j - i + 1) << "|" << value.substr(j - i, 1) << std::endl;

			}
			else if (key == "margin")
			{
				std::vector<std::string_view> sv;
				SplitByBlank(sv, value);
				int i = 0;
				if (i++ < sv.size())
				{
					const auto& v = sv.at(0);
					std::from_chars(v.data(), v.data() + v.size(), this->margin.left);
				}
				if (i++ < sv.size())
				{
					const auto& v = sv.at(1);
					std::from_chars(v.data(), v.data() + v.size(), this->margin.top);
				}
				if (i++ < sv.size())
				{
					const auto& v = sv.at(2);
					std::from_chars(v.data(), v.data() + v.size(), this->margin.right);
				}
				if (i++ < sv.size())
				{
					const auto& v = sv.at(3);
					std::from_chars(v.data(), v.data() + v.size(), this->margin.bottom);
				}
			}
			else if (key == "border")
			{
				std::vector<std::string_view> sv;
				SplitByBlank(sv, value);
				int i = 0;
				if (i++ < sv.size())
				{
					const auto& v = sv.at(0);
					std::from_chars(v.data(), v.data() + v.size(), this->border.left);
				}
				if (i++ < sv.size())
				{
					const auto& v = sv.at(1);
					std::from_chars(v.data(), v.data() + v.size(), this->border.top);
				}
				if (i++ < sv.size())
				{
					const auto& v = sv.at(2);
					std::from_chars(v.data(), v.data() + v.size(), this->border.right);
				}
				if (i++ < sv.size())
				{
					const auto& v = sv.at(3);
					std::from_chars(v.data(), v.data() + v.size(), this->border.bottom);
				}
			}
			else if (key == "style")
			{
				ReadStyle(value, style);
			}
			else if (key == "class")
			{
				std::vector<std::string_view> sv;
				SplitByBlank(sv, value);

				for (const auto& s : sv)
				{
					if (auto itr = style.find("." + std::string(s)); itr != style.end())
					{
						ReadStyle(itr->second, style);
					}
				}

			}
			else if (key == "id")
			{
				std::vector<std::string_view> sv;
				SplitByBlank(sv, value);

				for (const auto& s : sv)
				{
					if (auto itr = style.find("#" + std::string(s)); itr != style.end())
					{
						ReadStyle(itr->second, style);
					}
				}
			}
			else if (key == "background-color")
			{
				size_t i, j, len;
				len = key.size();
				//Both Trip
				for (i = 0; i < value.size(); ++i)
				{
					if (value[i] != ' ') break;
				}
				for (j = value.size(); j > 1; --j)
				{
					if (value[j - 1] != ' ') break;
				}
				if (value[i] == '#')
				{
					std::string s = value.substr(i + 1, j - i - 1);

					int hex = 0;

					sscanf_s(s.c_str(), "%x", &hex);
					
					background_color = DirectX::XMFLOAT4(
					(hex & 0xff) / 255.f,
					((hex >> 8) & 0xff) / 255.f,
					((hex >> 16) & 0xff) / 255.f, 1.f);

				}
			}
			else if (key == "border-color")
			{
				size_t i, j, len;
				len = key.size();
				//Both Trip
				for (i = 0; i < value.size(); ++i)
				{
					if (value[i] != ' ') break;
				}
				for (j = value.size(); j > 1; --j)
				{
					if (value[j - 1] != ' ') break;
				}
				if (value[i] == '#')
				{
					std::string s = value.substr(i + 1, j - i - 1);

					int hex = 0;

					sscanf_s(s.c_str(), "%x", &hex);

					border_color = DirectX::XMFLOAT4(
						(hex & 0xff) / 255.f,
						((hex >> 8) & 0xff) / 255.f,
						((hex >> 16) & 0xff) / 255.f, 1.f);

				}
			}
		}

		void ReadStyle(const std::string str, std::unordered_map<std::string, std::string>& style)
		{
			std::vector<std::string_view> sv;
			size_t start = -1;
			for (size_t i = 0; i < str.size(); ++i)
			{
				if (str.at(i) == ';')
				{
					if (start != -1)
					{
						sv.push_back(std::string_view(&str.at(start), i - start));
						start = -1;
					}
				}
				else if (start == -1) start = i;
			}
			if (start != -1)
			{
				sv.push_back(std::string_view(&str.at(start), str.size() - start));
				start = -1;
			}

			for (const auto& s : sv)
			{
				for (start = 0; start < s.size(); ++start) if (s.at(start) == ':') break;
				if (start != s.size())
				{
					size_t i = 0, j = 0, size;
					std::string header, trailer;
					std::string_view sv;


					size = 0;
					for (i = 0; i < start; ++i) if (s.at(i) != '\n' && s.at(i) != '\r') ++size;
					header.resize(size);
					j = 0;
					for (i = 0; i < start; ++i) if (s.at(i) != '\n' && s.at(i) != '\r') header.at(j++) = s.at(i);

					for (i = 0; i < header.size(); ++i)
					{
						if (header[i] != ' ') break;
					}
					for (j = header.size(); j > 1; --j)
					{
						if (header[j - 1] != ' ') break;
					}

					header = std::string_view(&header.at(i), j - i);


					size = 0;
					for (i = start + 1; i < s.size(); ++i) if (s.at(i) != '\n' && s.at(i) != '\r') ++size;
					trailer.resize(size);
					j = 0;
					for (i = start + 1; i < s.size(); ++i) if (s.at(i) != '\n' && s.at(i) != '\r') trailer.at(j++) = s.at(i);

					for (i = 0; i < trailer.size(); ++i)
					{
						if (trailer[i] != ' ') break;
					}
					for (j = trailer.size(); j > 1; --j)
					{
						if (trailer[j - 1] != ' ') break;
					}

					trailer = std::string_view(&trailer.at(i), j - i);

					tuple[header] = trailer;
					tupleChanged(header, style);
					//std::cout << "{" << header << "~" << trailer << "}" << std::endl;
				}
				else
				{
					size_t size = 0, i, j = 0;
					std::string header;

					size = 0;
					for (i = 0; i < start; ++i) if (s.at(i) != '\n' && s.at(i) != '\r') ++size;
					header.resize(size);
					j = 0;
					for (i = 0; i < start; ++i) if (s.at(i) != '\n' && s.at(i) != '\r') header.at(j++) = s.at(i);

					for (i = 0; i < header.size(); ++i)
					{
						if (header[i] != ' ') break;
					}
					for (j = header.size(); j > 1; --j)
					{
						if (header[j - 1] != ' ') break;
					}

					header = std::string_view(&header.at(i), j - i);
					//std::cout << "[" << header << "]" << std::endl;
					tuple[header] = "";
				}
			}
		}

	};


	struct Tree {
		Element value;
		std::vector<Tree*> child;

		[[nodiscard]] Element* operator-> () {
			return &value;
		}

		~Tree() {
			for (const auto p : child)
			{
				delete p;
			}
		}
	};
		
	inline void RawLoopTree_L(const std::function<void(Element&)>& func, Tree& tree)
	{
		func(tree.value);
		for (size_t _ = 0; _ < tree.child.size(); ++_)
		{
			RawLoopTree_L(func, *tree.child[_]);
		}
	}
	inline void RawLoopTree_L(const std::function<void(Element&, bool&)>& func, Tree& tree, bool& b)
	{
		func(tree.value, b);
		if (!b) return;
		for (size_t _ = 0; _ < tree.child.size(); ++_)
		{
			RawLoopTree_L(func, *tree.child[_], b);
			if (!b) return;
		}
	}

	inline void RawLoopTree_RL(const std::function<void(Element&)>& func, Tree& tree)
	{
		if (tree.child.size() > 0)
		{
			size_t _ = tree.child.size() - 1;
			do
			{
				RawLoopTree_RL(func, *tree.child[_]);
			} while (_-- != 0);
		}
		func(tree.value);
	}
	inline void RawLoopTree_RL(const std::function<void(Element&, bool&)>& func, Tree& tree, bool& b)
	{
		if (tree.child.size() > 0)
		{
			size_t _ = tree.child.size() - 1;
			do
			{
				RawLoopTree_RL(func, *tree.child[_], b);

				if (!b) return;
			} while (_-- != 0);
		}
		func(tree.value, b);
		if (!b) return;
	}

	inline void LoopTree_L(const std::function<FloatRect(Element&, FloatRect&, bool&)>& func, Tree& tree, FloatRect& rect, bool& run)
	{
		auto r = func(tree.value, rect, run);
		if (!run) return;
		for (size_t _ = 0; _ < tree.child.size(); ++_)
		{
			//std::cout << "index : " << _ << " / " << tree.child.size() << std::endl;
			LoopTree_L(func, *tree.child[_], rect, run);
			if (!run) return;
		}
		rect = r;
	}
	inline void LoopTree_L(const std::function<FloatRect(Element&, FloatRect&, bool&)>& func, Tree& tree)
	{
		bool run = true;
		FloatRect rect = { 0.f, 0.f, tree->size.w, tree->size.h };
		auto r = func(tree.value, rect, run);
		if (!run) return;
		
		for (size_t _ = 0; _ < tree.child.size(); ++_)
		{
			//std::cout << "index : " << _ << " / " << tree.child.size() << std::endl;
			LoopTree_L(func, *tree.child[_], rect, run);
			if (!run) return;
		}
	}

	inline void RunYTML1_1(YTML1_1::Tree& MainDisplay, const std::function<void(Element&, bool&)>& user_func)
	{
		YTML1_1::LoopTree_L([&](YTML1_1::Element & t, YTML1_1::FloatRect r, bool& run)
			{
				YTML1_1::FloatRect rect = r;
				switch (t.halign)
				{
				case YTML1_1::ElementHorizontalAlign::Left:
					rect.x += t.margin.left;
					rect.w -= t.margin.left;
					break;
				case YTML1_1::ElementHorizontalAlign::Center:
					rect.x += (r.w - t.size.w) / 2;
					rect.w -= t.size.w;
					break;
				case YTML1_1::ElementHorizontalAlign::Right:
					rect.w -= t.margin.right;
					break;
				}
				switch (t.valign)
				{
				case YTML1_1::ElementVerticalAlign::Top:
					rect.y += t.margin.top;
					rect.h -= t.margin.top;
					break;
				case YTML1_1::ElementVerticalAlign::Middle:
					rect.y += (r.h - t.size.h) / 2;
					rect.h -= t.size.h;
					break;
				case YTML1_1::ElementVerticalAlign::Bottom:
					rect.h -= t.margin.bottom;
					break;
				}

				//[Will]Check Overflow
				rect.w = t.size.w;
				rect.h = t.size.h;
				//
				t.size_in_display = rect;
				user_func(t, run);

				//Parent Clip
				switch (t.pclip)
				{
				case ElementParentClipDirection::Horizontal:
					switch (t.halign)
					{
					case YTML1_1::ElementHorizontalAlign::Left:
						r.x += t.margin.left + rect.w;
						r.w -= t.margin.left + rect.w;
						break;
					case YTML1_1::ElementHorizontalAlign::Right:
						r.w -= t.margin.right + rect.w;
						break;
					}
					break;
				case ElementParentClipDirection::Vertical:
					switch (t.valign)
					{
					case YTML1_1::ElementVerticalAlign::Top:
						r.y += t.margin.top + rect.h;
						r.h -= t.margin.top + rect.h;
						break;
					case YTML1_1::ElementVerticalAlign::Bottom:
						r.h -= t.margin.bottom + rect.h;
						break;
					}
					break;
				}
				return r;
			},
			MainDisplay);
	}

	inline bool PossibleVariablename(const char& c)
	{
		return c == '_' || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
	}

	inline void ParseInnerBracket(YTML1_1::Element & e, const std::string_view& s, std::unordered_map<std::string, std::string>& style)
	{
		std::string_view header = "";
		std::string key;
		size_t last = 0, i;
		char c;

		//Get header
		for (i = 0; i < s.size(); ++i)
		{
			c = s[i];
			if (c == ' ') {
				header = std::string_view(&s.at(last), i - last);
				last = i + 1;
				e.head = header;
				break;
			}
		}

		if (last > 0 && last < s.size())
		{
			//Get Trainer
			bool wait_for_equal_sign = true;
			size_t str_index = -1;
			bool str_open = false;
			bool wait_for_varname = true;

			for (i = last; i < s.size(); ++i)
			{
				c = s[i];
				if (wait_for_equal_sign) {
					if (PossibleVariablename(c))
					{
						if (!wait_for_varname)
						{
							last = i;
							wait_for_varname = true;
						}
					}
					else if (c == ' ') {
						wait_for_varname = false;
					}
					else if (c == '=') {
						size_t end = i;
						for (; s.at(end - 1) == ' '; --end);
						//Get key
						key = s.substr(last, end - last);
						wait_for_equal_sign = false;
					}
				}
				else {
					if (c == '"') {
						if (str_open) {
							//Get value

							e.tuple[key] = s.substr(str_index, i - str_index);
							e.tupleChanged(key, style);

							wait_for_equal_sign = true;
							last = i + 1;
						}
						else {
							str_index = i + 1;
						}
						str_open = !str_open;
					}
				}
			}
		}
		else
		{
			header = s;
		}
	}

	inline void ReadYTML1_1(const std::string& path, YTML1_1::Tree& MainDisplay, std::unordered_map<std::string, std::string>& style, size_t& biggest_id)
	{
		std::ifstream file(path);
		std::vector<size_t> ind;
		
		std::vector<YTML1_1::Tree*> Parent;
		Parent.push_back(&MainDisplay);


		if (file.bad()) return;

		std::string str((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

		file.close();

		bool forward_close = false;
		bool back_close = false;

		char c;
		size_t depth = 0;
		for (size_t i = 0; i < str.size(); ++i)
		{
			c = str[i];
			switch (c)
			{
			case '<':
				//cout << c;
				ind.push_back(i + 1);

				if (str[i + 1] == '/') forward_close = true;
				break;
			case '>':

				if (forward_close)
				{
					--depth;
					//cout << '/' << depth << c;
					Parent.pop_back();
				}
				else if (back_close)
				{
					//cout << depth << '/' << c;
					YTML1_1::Tree* e = new YTML1_1::Tree();
					e->value.eid = biggest_id++;

					ParseInnerBracket(e->value, std::string_view(&str.at(*ind.crbegin()), i - *ind.crbegin() - 1), style);
					//std::cout << "( " << e->value.size.w << ", " << e->value.size.h << " )" << std::endl;
					(*Parent.rbegin())->child.push_back(e);
				}
				else
				{
					//cout << depth << c;
					YTML1_1::Tree* e = new YTML1_1::Tree();
					e->value.eid = biggest_id++;

					ParseInnerBracket(e->value, std::string_view(&str.at(*ind.crbegin()), i - *ind.crbegin()), style);
					//std::cout << "( " << e->value.size.w << ", " << e->value.size.h << " )" << std::endl;

					(*Parent.rbegin())->child.push_back(e);
					Parent.push_back(e);


					++depth;
				}
				ind.pop_back();

				forward_close = false;
				back_close = false;
				break;
			case '/':
				if (str[i + 1] == '>') back_close = true;
				break;
			}
		}
	}

}
