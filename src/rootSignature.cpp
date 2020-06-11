#include <charconv>

#include <d3dx12.h>

#include <agz/d3d12/pipeline/rootSignature.h>
#include <agz/utility/string.h>

AGZ_D3D12_BEGIN

namespace
{
    /*
        # this is a comment

        RootSignature -> (RootSignatureUnit;)*

        RootSignatureUnit -> RootParameter
        RootSignatureUnit -> StaticSampler
        RootSignatureUnit -> Flags (, Flags)*
        
        RootParameter -> s Integer b Integer (, ShaderVisibility)? : CBV 
        RootParameter -> s Integer b Integer (, ShaderVisibility)? : Cons, num32Bits
        RootParameter -> s Integer u Integer (, ShaderVisibility)? : UAV
        RootParameter -> { (Range;)* } (, ShaderVisibility)?
        
        Range -> s Integer b Integer : CBV RangeSize?
        Range -> s Integer t Integer : SRV RangeSize?
        Range -> s Integer u Integer : UAV RangeSize?
        
        RangeSize -> [ Integer ]
        
        ShaderVisibility -> vertex
        ShaderVisibility -> pixel
        ShaderVisibility -> all

        RootSignature -> (RootSignatureUnit;)*

        RootSignatureUnit -> RootParameter
        RootSignatureUnit -> StaticSampler
        
        RootParameter -> s Integer b Integer (, ShaderVisibility)? : CBV
        RootParameter -> s Integer b Integer (, ShaderVisibility)? : Cons, num32Bits
        RootParameter -> s Integer u Integer (, ShaderVisibility)? : UAV
        RootParameter -> ShaderVisibility? { (Range;)* }
        
        Range -> s Integer b Integer : CBV RangeSize?
        Range -> s Integer t Integer : SRV RangeSize?
        Range -> s Integer u Integer : UAV RangeSize?
        
        RangeSize -> [ Integer ]
        
        ShaderVisibility -> vertex
        ShaderVisibility -> pixel
        ShaderVisibility -> all
        
        StaticSampler -> s Integer s Integer (, ShaderVisibility)? : { (StaticSamplerArgument;)* }
        
        StaticSamplerArgument -> filter : anisotropic
        StaticSamplerArgument -> filter : xxx, xxx, xxx (xxx = nearest/linear/point)
        StaticSamplerArgument -> address : xxx, xxx, xxx (xxx = clamp/wrap/mirror)
        StaticSamplerArgument -> mipLODBias : Float
        StaticSamplerArgument -> maxAnisotropy : Integer
        StaticSamplerArgument -> comparisonFunc : always/never/equal/notEqual/less/greater/lessEqual/greaterEqual
        StaticSamplerArgument -> minLOD : Float
        StaticSamplerArgument -> maxLOD : Float

        Flags -> inputAssembly
        Flags -> streamOutput
        Flags -> noVertex
        Flags -> noHull
        Flags -> noDomain
        Flags -> noGeometry
        Flags -> noPixel
     */

    class ParserException : public D3D12LabException
    {
    public:

        using D3D12LabException::D3D12LabException;
    };

    enum class TokenType
    {
        BeginScope = 0, // {
        EndScope   = 1, // }
        BeginSize  = 2, // [
        EndSize    = 3, // ]
        Colon      = 4, // :
        Number     = 5, // 123456.789
        Name       = 6, // abcde
        Comma      = 7, // ,
        Semicolon  = 8, // ;
        EndMark    = 9
    };

    struct Token
    {
        using Type = TokenType;

        Type type = Type::EndMark;
        std::string name;

        bool is(Type t) const noexcept
        {
            return type == t;
        }

        std::string toStr() const
        {
            const char *NAMES[] = {
                "{", "}", "[", "]", ":", "", "",  ",", ";", "$"
            };
            if(type == Type::Number)
                return "Integer{" + name + "}";
            if(type == Type::Name)
                return "Name{" + name + "}";
            return NAMES[static_cast<int>(type)];
        }
    };

    class Tokenizer
    {
        std::string src_;
        size_t idx_;

        int curLine_ = 0;

        void skipWhitespacesAndComments()
        {
            while(idx_ < src_.size() && std::isspace(src_[idx_]))
            {
                if(src_[idx_] == '\n')
                    ++curLine_;
                ++idx_;
            }

            if(idx_ >= src_.size())
                return;

            if(src_[idx_] == '#')
            {
                ++idx_;
                while(idx_ < src_.size() && src_[idx_] != '\n')
                    ++idx_;
                skipWhitespacesAndComments();
            }
        }
        
        Token parseNext()
        {
            // skip whitespaces & comments

            skipWhitespacesAndComments();

            // endmark

            if(idx_ >= src_.size())
                return Token{ TokenType::EndMark, {} };

            // symbols

            const char LA = src_[idx_++];
            switch(LA)
            {
            case '{': return Token{ TokenType::BeginScope, {} };
            case '}': return Token{ TokenType::EndScope  , {} };
            case '[': return Token{ TokenType::BeginSize , {} };
            case ']': return Token{ TokenType::EndSize   , {} };
            case ':': return Token{ TokenType::Colon     , {} };
            case ',': return Token{ TokenType::Comma     , {} };
            case ';': return Token{ TokenType::Semicolon , {} };
            default: break;
            }

            // number

            float floatV;
            const auto [floatP, floatE] = std::from_chars(
                &src_[idx_ - 1], &src_[src_.length()], floatV);

            if(floatE == std::errc())
            {
                const char *pStart = &src_[idx_ - 1];
                const std::string numStr(pStart, floatP - pStart);
                idx_ = floatP - &src_[0];

                return { TokenType::Number, numStr };
            }

            // name

            if(!std::isalpha(LA) && LA != '_')
            {
                err("unknown symbol: " + std::string(1, LA));
            }

            std::string name(1, LA);
            while(idx_ < src_.size() && (std::isalnum(src_[idx_]) ||
                                         src_[idx_] == '_'))
                name += src_[idx_++];

            return { TokenType::Name, std::move(name) };
        }

    public:

        Token cur;

        explicit Tokenizer(std::string src)
            : src_(std::move(src)), idx_(0)
        {
            next();
        }

        bool match(Token::Type t)
        {
            if(!cur.is(t))
                return false;
            next();
            return true;
        }

        bool matchName(std::string_view n)
        {
            if(cur.is(TokenType::Name) && cur.name == n)
            {
                next();
                return true;
            }
            return false;
        }

        void matchOrErr(Token::Type t, std::string_view errMsg)
        {
            if(!match(t))
                err(errMsg);
        }

        void matchNameOrErr(std::string_view n, std::string_view errMsg)
        {
            if(cur.is(TokenType::Name) && cur.name == n)
                next();
            else
                err(errMsg);
        }

        void next()
        {
            cur = parseNext();
        }

        [[noreturn]] void err(std::string_view msg) const
        {
            throw ParserException(
                "syntax error at line " + std::to_string(curLine_) +
                ". " + std::string(msg) +
                ". next token: " + cur.toStr());
        }
    };

    UINT parseRangeSize(Tokenizer &toks)
    {
        toks.matchOrErr(TokenType::BeginSize, "'[' expected");

        if(!toks.cur.is(TokenType::Number))
            toks.err("number of descriptors expected");

        UINT ret;
        try
        {
            ret = std::stoul(toks.cur.name);
        }
        catch(...)
        {
            toks.err("invalid integer format: " + toks.cur.name);
        }
        toks.next();

        toks.matchOrErr(TokenType::EndSize, "']' expected");

        return ret;
    }

    bool parseRegister(
        Tokenizer &toks, char registerName,
        UINT &registerSpace, UINT &registerNum)
    {
        if(!toks.cur.is(Token::Type::Name))
            return false;

        if(toks.cur.name[0] != 's')
            return false;

        const auto ss = stdstr::split(toks.cur.name.substr(1), registerName);
        if(ss.size() != 2)
            return false;

        try
        {
            registerSpace = std::stoul(ss[0]);
            registerNum   = std::stoul(ss[1]);
        }
        catch(...)
        {
            return false;
        }

        toks.next();
        return true;
    }

    D3D12_DESCRIPTOR_RANGE parseRange(Tokenizer &toks)
    {
        D3D12_DESCRIPTOR_RANGE range;
        range.OffsetInDescriptorsFromTableStart = 0;

        if(parseRegister(
            toks, 'b', range.RegisterSpace, range.BaseShaderRegister))
        {
            range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;

            toks.matchOrErr(TokenType::Colon, "':' expected");
            toks.matchNameOrErr("CBV", "'CBV' expected");

            if(toks.cur.is(TokenType::BeginSize))
                range.NumDescriptors = parseRangeSize(toks);
            else
                range.NumDescriptors = 1;

            return range;
        }

        if(parseRegister(
            toks, 't', range.RegisterSpace, range.BaseShaderRegister))
        {
            range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

            toks.matchOrErr(TokenType::Colon, "':' expected");
            toks.matchNameOrErr("SRV", "'SRV' expected");

            if(toks.cur.is(TokenType::BeginSize))
                range.NumDescriptors = parseRangeSize(toks);
            else
                range.NumDescriptors = 1;

            return range;
        }

        if(parseRegister(
            toks, 'u', range.RegisterSpace, range.BaseShaderRegister))
        {
            range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;

            toks.matchOrErr(TokenType::Colon, "':' expected");
            toks.matchNameOrErr("UAV", "'UAV' expected");

            if(toks.cur.is(TokenType::BeginSize))
                range.NumDescriptors = parseRangeSize(toks);
            else
                range.NumDescriptors = 1;

            return range;
        }

        toks.err("unknown range type");
    }

    D3D12_SHADER_VISIBILITY parseShaderVisibility(Tokenizer &toks)
    {
        if(!toks.cur.is(Token::Type::Name))
            toks.err("shader visibility expected");

        AGZ_SCOPE_GUARD({ toks.next(); });
        if(toks.cur.name == "vertex")
            return D3D12_SHADER_VISIBILITY_VERTEX;
        if(toks.cur.name == "pixel")
            return D3D12_SHADER_VISIBILITY_PIXEL;
        if(toks.cur.name == "all")
            return D3D12_SHADER_VISIBILITY_ALL;

        toks.err("unknown shader visibility: " + toks.cur.name);
    }

    D3D12_ROOT_PARAMETER parseRootParameter(
        Tokenizer &toks, RootSignatureParamPool &paramPool)
    {
        // descriptor table


        // CBV

        UINT registerSpace, registerNum;
        if(parseRegister(toks, 'b', registerSpace, registerNum))
        {
            D3D12_SHADER_VISIBILITY vis;
            if(toks.match(TokenType::Comma))
                vis = parseShaderVisibility(toks);
            else
                vis = D3D12_SHADER_VISIBILITY_ALL;

            toks.matchOrErr(TokenType::Colon, "':' expected");

            if(!toks.cur.is(TokenType::Name))
                toks.err("'CBV'/'Cons' expected");
            
            if(toks.cur.name == "CBV")
            {
                toks.next();

                CD3DX12_ROOT_PARAMETER ret;
                ret.InitAsConstantBufferView(registerNum, registerSpace, vis);
                return ret;
            }

            if(toks.cur.name == "Cons")
            {
                toks.next();
                toks.matchOrErr(TokenType::Comma, "',' expected");

                if(!toks.cur.is(TokenType::Number))
                    toks.err("num32Bits expected");

                UINT num32Bits;
                try
                {
                    num32Bits = std::stoul(toks.cur.name);
                }
                catch(...)
                {
                    toks.err("invalid num32Bits format: " + toks.cur.name);
                }
                toks.next();

                CD3DX12_ROOT_PARAMETER ret;
                ret.InitAsConstants(num32Bits, registerNum, registerSpace, vis);
                return ret;
            }

            toks.err("unknown sxbx root parameter type: " + toks.cur.name);
        }

        // UAV

        if(parseRegister(toks, 'u', registerSpace, registerNum))
        {
            D3D12_SHADER_VISIBILITY vis;
            if(toks.match(TokenType::Comma))
                vis = parseShaderVisibility(toks);
            else
                vis = D3D12_SHADER_VISIBILITY_ALL;

            toks.matchOrErr(TokenType::Colon, "':' expected");
            toks.matchNameOrErr("UAV", "'UAV' expected");

            CD3DX12_ROOT_PARAMETER ret;
            ret.InitAsUnorderedAccessView(registerNum, registerSpace, vis);
            return ret;
        }

        // table

        D3D12_SHADER_VISIBILITY vis;
        if(!toks.cur.is(TokenType::BeginScope))
        {
            vis = parseShaderVisibility(toks);
            toks.matchOrErr(TokenType::Colon, "':' expected");
        }
        else
            vis = D3D12_SHADER_VISIBILITY_ALL;

        toks.matchOrErr(TokenType::BeginScope, "'{' expected");

        std::vector<D3D12_DESCRIPTOR_RANGE> tempRanges;
        while(!toks.match(TokenType::EndScope))
        {
            tempRanges.push_back(parseRange(toks));
            toks.matchOrErr(TokenType::Semicolon, "';' expected");
        }

        UINT offsetFromStart = 0;
        for(auto &r : tempRanges)
        {
            r.OffsetInDescriptorsFromTableStart = offsetFromStart;
            offsetFromStart += r.NumDescriptors;
        }

        auto ranges = paramPool.allocRange(tempRanges.size());
        std::memcpy(
            ranges, tempRanges.data(),
            sizeof(D3D12_DESCRIPTOR_RANGE) * tempRanges.size());

        CD3DX12_ROOT_PARAMETER ret;
        ret.InitAsDescriptorTable(UINT(tempRanges.size()), ranges, vis);
        return ret;
    }

    void parseStaticSamplerArgument(
        Tokenizer &toks, D3D12_STATIC_SAMPLER_DESC &desc)
    {
        if(toks.matchName("filter"))
        {
            toks.matchOrErr(TokenType::Colon, "':' expected");

            if(toks.matchName("anisotropic"))
            {
                desc.Filter = D3D12_FILTER_ANISOTROPIC;
                return;
            }

            auto isNextLinear = [&]
            {
                if(toks.matchName("linear"))
                    return true;
                if(toks.matchName("nearest") || toks.matchName("point"))
                    return false;
                toks.err("'linear'/'nearest'/'point' expected");
            };

            const bool minLinear = isNextLinear();
            toks.matchOrErr(TokenType::Comma, "',' expected");
            const bool magLinear = isNextLinear();
            toks.matchOrErr(TokenType::Comma, "',' expected");
            const bool mipLinear = isNextLinear();


            static const D3D12_FILTER RESULT_FILTER[] = {
                D3D12_FILTER_MIN_MAG_MIP_POINT,
                D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR,
                D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT,
                D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR,
                D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT,
                D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR,
                D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT,
                D3D12_FILTER_MIN_MAG_MIP_LINEAR
            };

            const int idx = (minLinear ? 4 : 0) +
                            (magLinear ? 2 : 0) +
                            (mipLinear ? 1 : 0);
            desc.Filter = RESULT_FILTER[idx];

            return;
        }

        if(toks.matchName("address"))
        {
            toks.matchOrErr(TokenType::Colon, "':' expected");

            auto nextAddressMode = [&]
            {
                if(toks.matchName("clamp"))
                    return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
                if(toks.matchName("wrap"))
                    return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
                if(toks.matchName("mirror"))
                    return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
                toks.err("invalid textureaddress mode");
            };

            desc.AddressU = nextAddressMode();
            toks.matchOrErr(TokenType::Comma, "',' expected");
            desc.AddressV = nextAddressMode();
            toks.matchOrErr(TokenType::Comma, "',' expected");
            desc.AddressW = nextAddressMode();

            return;
        }

        if(toks.matchName("mipLODBias"))
        {
            toks.matchOrErr(TokenType::Colon, "':' expected");

            if(!toks.cur.is(TokenType::Number))
                toks.err("float number expected");
            desc.MipLODBias = std::stof(toks.cur.name);

            toks.next();
            return;
        }

        if(toks.matchName("maxAnisotropy"))
        {
            toks.matchOrErr(TokenType::Colon, "':' expected");

            if(!toks.cur.is(TokenType::Number))
                toks.err("integer number expected");

            try
            {
                desc.MaxAnisotropy = std::stoul(toks.cur.name);
            }
            catch(...)
            {
                toks.err("invalid maxAnisotropy format: " + toks.cur.name);
            }

            toks.next();
            return;
        }

        if(toks.matchName("comparisonFunc"))
        {
            toks.matchOrErr(TokenType::Colon, "':' expected");

            // IMPROVE: performace optimization
            if(toks.matchName("always"))
                desc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
            else if(toks.matchName("never"))
                desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
            else if(toks.matchName("equal"))
                desc.ComparisonFunc = D3D12_COMPARISON_FUNC_EQUAL;
            else if(toks.matchName("notEqual"))
                desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NOT_EQUAL;
            else if(toks.matchName("less"))
                desc.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS;
            else if(toks.matchName("greater"))
                desc.ComparisonFunc = D3D12_COMPARISON_FUNC_GREATER;
            else if(toks.matchName("lessEqual"))
                desc.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
            else if(toks.matchName("greaterEqual"))
                desc.ComparisonFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
            else
                toks.err("unknown comparison function: " + toks.cur.name);

            return;
        }

        if(toks.matchName("minLOD"))
        {
            toks.matchOrErr(TokenType::Colon, "':' expected");

            if(!toks.cur.is(TokenType::Number))
                toks.err("float number expected");
            desc.MinLOD = std::stof(toks.cur.name);

            toks.next();
            return;
        }

        if(toks.matchName("maxLOD"))
        {
            toks.matchOrErr(TokenType::Colon, "':' expected");

            if(!toks.cur.is(TokenType::Number))
                toks.err("float number expected");
            desc.MaxLOD = std::stof(toks.cur.name);

            toks.next();
            return;
        }

        toks.err("unknown argument format");
    }

    bool parseStaticSampler(Tokenizer &toks, D3D12_STATIC_SAMPLER_DESC &desc)
    {
        if(!parseRegister(toks, 's', desc.ShaderRegister, desc.RegisterSpace))
            return false;

        if(toks.match(TokenType::Comma))
            desc.ShaderVisibility = parseShaderVisibility(toks);
        else
            desc.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        toks.matchOrErr(TokenType::Colon, "':' expected");
        toks.matchOrErr(TokenType::BeginScope, "'{' expected");

        desc.Filter           = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        desc.AddressU         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        desc.AddressV         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        desc.AddressW         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        desc.MipLODBias       = 0;
        desc.MaxAnisotropy    = 16;
        desc.ComparisonFunc   = D3D12_COMPARISON_FUNC_ALWAYS;
        desc.BorderColor      = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        desc.MinLOD           = 0;
        desc.MaxLOD           = FLT_MAX;
        
        while(!toks.match(TokenType::EndScope))
        {
            parseStaticSamplerArgument(toks, desc);
            toks.matchOrErr(TokenType::Semicolon, "';' expected");
        }

        return true;
    }

    bool parseFlags(Tokenizer &toks, D3D12_ROOT_SIGNATURE_FLAGS &flags)
    {
        auto matchOne = [&](const char *name, D3D12_ROOT_SIGNATURE_FLAGS newFlag)
        {
            if(toks.matchName(name))
            {
                flags |= newFlag;
                return true;
            }
            return false;
        };

        auto match = [&]
        {
            static const std::pair<
                const char*, D3D12_ROOT_SIGNATURE_FLAGS> flagPairs[] =
            {
                { "inputAssembly", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT },
                { "streamOutput" , D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT                },
                { "noVertex"     , D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS     },
                { "noHull"       , D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS       },
                { "noDomain"     , D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS     },
                { "noGeometry"   , D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS   },
                { "noPixel"      , D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS      }
            };

            for(auto &p : flagPairs)
            {
                if(matchOne(p.first, p.second))
                    return true;
            }
            return false;
        };

        if(match())
        {
            while(toks.match(TokenType::Comma))
            {
                if(!match())
                    toks.err("unknown flag name");
            }
            return true;
        }

        return false;
    }

    D3D12_ROOT_SIGNATURE_DESC parse(
        Tokenizer &toks, RootSignatureParamPool &paramPool)
    {
        std::vector<D3D12_ROOT_PARAMETER>      tempParams;
        std::vector<D3D12_STATIC_SAMPLER_DESC> tempSamplers;

        D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

        while(!toks.cur.is(TokenType::EndMark))
        {
            D3D12_STATIC_SAMPLER_DESC samplerDesc;
            if(parseStaticSampler(toks, samplerDesc))
                tempSamplers.push_back(samplerDesc);
            else if(!parseFlags(toks, flags))
                tempParams.push_back(parseRootParameter(toks, paramPool));

            toks.matchOrErr(TokenType::Semicolon, "';' expected");
        }

        auto params = paramPool.allocParams(tempParams.size());
        std::memcpy(
            params, tempParams.data(),
            sizeof(D3D12_ROOT_PARAMETER) * tempParams.size());

        auto samplers = paramPool.allocSamplers(tempSamplers.size());
        std::memcpy(
            samplers, tempSamplers.data(),
            sizeof(D3D12_STATIC_SAMPLER_DESC) * tempSamplers.size());

        CD3DX12_ROOT_SIGNATURE_DESC ret;
        ret.Init(
            UINT(tempParams.size()), params,
            UINT(tempSamplers.size()), samplers,
            flags);

        return ret;
    }
}

RootSignatureBuilder::RootSignatureBuilder(std::string rootSignature)
    : desc_{}
{
    Tokenizer toks(std::move(rootSignature));
    desc_ = parse(toks, pool_);
}

const D3D12_ROOT_SIGNATURE_DESC &RootSignatureBuilder::getDesc() const noexcept
{
    return desc_;
}

ComPtr<ID3D12RootSignature> RootSignatureBuilder::createSignature(
    ID3D12Device *device) const
{
    ComPtr<ID3D10Blob> rootSignatureBlob;
    AGZ_D3D12_CHECK_HR(D3D12SerializeRootSignature(
        &desc_, D3D_ROOT_SIGNATURE_VERSION_1,
        rootSignatureBlob.GetAddressOf(), nullptr));

    ComPtr<ID3D12RootSignature> ret;
    AGZ_D3D12_CHECK_HR(device->CreateRootSignature(
        0,
        rootSignatureBlob->GetBufferPointer(),
        rootSignatureBlob->GetBufferSize(),
        IID_PPV_ARGS(ret.GetAddressOf())));

    return ret;
}

ComPtr<ID3D12RootSignature> createRootSignature(
    ID3D12Device *device, std::string rootSignature)
{
    return RootSignatureBuilder(std::move(rootSignature))
                .createSignature(device);
}

AGZ_D3D12_END
