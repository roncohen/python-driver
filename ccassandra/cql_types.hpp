#ifndef __PYCCASSANDRA_CQLTYPES
#define __PYCCASSANDRA_CQLTYPES
#include <vector>
#include <string>
#include "buffer.hpp"
#include "python.hpp"


namespace pyccassandra
{
    /// CQL type.
    class CqlType
    {
    public:
        CqlType() {}
        virtual ~CqlType();


        /// Deserialize the data type from a buffer.

        /// @param buffer Buffer.
        /// @param protocolVersion Protocol version.
        /// @returns a pointer to the deserialized Python object representation
        /// of the value if successful, otherwise NULL, in which case the
        /// proper Python exception has been set.
        virtual PyObject* Deserialize(Buffer& buffer,
                                      int protocolVersion) = 0;


        virtual PyObject* Empty()
        {
            Py_INCREF(Py_None);
            return Py_None;
        }
    private:
        CqlType(CqlType&);
        CqlType& operator =(CqlType&);
    };


#define DECLARE_SIMPLE_CQL_TYPE_CLASS(_cls)                        \
    class _cls                                                     \
        :   public CqlType                                         \
    {                                                              \
    public:                                                        \
        _cls()                                                     \
            :   CqlType()                                          \
        {}                                                         \
        virtual ~_cls() {}                                         \
        virtual PyObject* Deserialize(Buffer& buffer,              \
                                      int protocolVersion);        \
    }


    /// 32-bit signed integer CQL type.
    DECLARE_SIMPLE_CQL_TYPE_CLASS(CqlInt32Type);


    /// 64-bit signed integer CQL type.
    DECLARE_SIMPLE_CQL_TYPE_CLASS(CqlLongType);


    /// Counter column CQL type.
    typedef CqlLongType CqlCounterColumnType;


    /// 32-bit floating point CQL type.
    DECLARE_SIMPLE_CQL_TYPE_CLASS(CqlFloatType);


    /// 64-bit floating point CQL type.
    DECLARE_SIMPLE_CQL_TYPE_CLASS(CqlDoubleType);


    /// Boolean CQL type.
    DECLARE_SIMPLE_CQL_TYPE_CLASS(CqlBooleanType);


    /// Bytes CQL type.
    DECLARE_SIMPLE_CQL_TYPE_CLASS(CqlBytesType);


    /// ASCII CQL type.
    typedef CqlBytesType CqlAsciiType;


    /// UTF-8 CQL type.
    DECLARE_SIMPLE_CQL_TYPE_CLASS(CqlUtf8Type);


    /// Varchar CQL type.
    typedef CqlUtf8Type CqlVarcharType;


    /// Inet address CQL type.
    DECLARE_SIMPLE_CQL_TYPE_CLASS(CqlInetAddressType);


    /// Integer CQL type (varint.)
    DECLARE_SIMPLE_CQL_TYPE_CLASS(CqlIntegerType);


    /// UUID CQL type.
    class CqlUuidType
        :   public CqlType
    {
    public:
        CqlUuidType(PyObject* pythonUuidType)
            :   CqlType(),
                _pythonUuidType(pythonUuidType)
        {}
        virtual ~CqlUuidType() {}
        virtual PyObject* Deserialize(Buffer& buffer,
                                      int protocolVersion);
    private:
        PyObject* _pythonUuidType;
    };


    /// Date CQL type.
    class CqlDateType
        :   public CqlType
    {
    public:
        CqlDateType(PyObject* pyDatetimeDatetimeUtcFromTimestamp)
            :   CqlType(),
                _pyDatetimeDatetimeUtcFromTimestamp(
                    pyDatetimeDatetimeUtcFromTimestamp
                )
        {}
        virtual ~CqlDateType() {}
        virtual PyObject* Deserialize(Buffer& buffer,
                                      int protocolVersion);
    private:
        PyObject* _pyDatetimeDatetimeUtcFromTimestamp;
    };


    /// Timestamp CQL type.
    typedef CqlDateType CqlTimestampType;


    /// Decimal CQL type.
    class CqlDecimalType
        :   public CqlType
    {
    public:
        CqlDecimalType(PyObject* pythonDecimalType)
            :   CqlType(),
                _pythonDecimalType(pythonDecimalType)
        {}
        virtual ~CqlDecimalType() {}
        virtual PyObject* Deserialize(Buffer& buffer,
                                      int protocolVersion);
    private:
        PyObject* _pythonDecimalType;
    };


    /// Tuple CQL type.
    class CqlTupleType
        :   public CqlType
    {
    public:
        /// Initialize a tuple CQL type.

        /// @param subtypes Subtypes. The tuple type takes over ownership of
        /// the references, and they should therefore *not* be released by
        /// the caller. This is not enforced, so be wary.
        CqlTupleType(const std::vector<CqlType*>& subtypes);


        virtual ~CqlTupleType();
        virtual PyObject* Deserialize(Buffer& buffer,
                                      int protocolVersion);
    private:
        std::vector<CqlType*> _subtypes;
    };


    /// List CQL type.
    class CqlListType
        :   public CqlType
    {
    public:
        /// Initialize a list CQL type.

        /// @param itemType Item type. The list type takes over ownership of
        /// the reference, and it should therefore *not* be released by the
        /// caller. This is not enforced, so be wary.
        CqlListType(CqlType* itemType);


        virtual ~CqlListType();
        virtual PyObject* Deserialize(Buffer& buffer,
                                      int protocolVersion);
    private:
        CqlType* _itemType;
    };


    /// Set CQL type.
    class CqlSetType
        :   public CqlType
    {
    public:
        /// Initialize a set CQL type.

        /// @param itemType Item type. The set type takes over ownership of
        /// the reference, and it should therefore *not* be released by the
        /// caller. This is not enforced, so be wary.
        CqlSetType(CqlType* itemType, PyObject* pySetType);


        virtual ~CqlSetType();
        virtual PyObject* Deserialize(Buffer& buffer,
                                      int protocolVersion);
    private:
        CqlType* _itemType;
        PyObject* _pySetType;
    };


    /// Map CQL type.
    class CqlMapType
        :   public CqlType
    {
    public:
        /// Initialize a map CQL type.

        /// @param keyType Key type. The map type takes over ownership of
        /// the reference, and it should therefore *not* be released by the
        /// caller. This is not enforced, so be wary.
        /// @param valueType Value type. The map type takes over ownership of
        /// the reference, and it should therefore *not* be released by the
        /// caller. This is not enforced, so be wary.
        CqlMapType(CqlType* keyType,
                   CqlType* valueType,
                   PyObject* pyMapType);


        virtual ~CqlMapType();
        virtual PyObject* Deserialize(Buffer& buffer,
                                      int protocolVersion);
    private:
        CqlType* _keyType;
        CqlType* _valueType;
        PyObject* _pyMapType;
    };


    /// Proxy CQL type.
    class CqlProxyType
        :   public CqlType
    {
    public:
        /// Initialize a proxy CQL type.

        /// @param wrappedType Wrapped type. The proxy type takes over
        /// ownership of the reference, and it should therefore *not* be
        /// released by the caller. This is not enforced, so be wary.
        CqlProxyType(CqlType* wrappedType);


        virtual ~CqlProxyType();


        virtual PyObject* Deserialize(Buffer& buffer, int protocolVersion);
    private:
        CqlType* _wrappedType;
    };


    /// Frozen CQL type.
    class CqlFrozenType
        :   public CqlProxyType
    {
    public:
        /// Initialize a frozen CQL type.

        /// @param wrappedType Wrapped type. The frozen type takes over
        /// ownership of the reference, and it should therefore *not* be
        /// released by the caller. This is not enforced, so be wary.
        CqlFrozenType(CqlType* wrappedType);
    };


    /// Reversed CQL type.
    class CqlReversedType
        :   public CqlProxyType
    {
    public:
        /// Initialize a reversed CQL type.

        /// @param wrappedType Wrapped type. The reversed type takes over
        /// ownership of the reference, and it should therefore *not* be
        /// released by the caller. This is not enforced, so be wary.
        CqlReversedType(CqlType* wrappedType);
    };


    /// User type.
    class CqlUserType
        :   public CqlType
    {
    public:
        /// Names and type vector.
        typedef std::vector<std::pair<std::string, CqlType*> >
            NamesAndTypeVector;

        
        /// Initialize a user CQL type.

        /// @param namesAndTypes Names and types of the user type's fields.
        /// The user type takes over the ownership of the references, and they
        /// should therefore *not* be released by the caller. This is not
        /// enforced, so be wary.
        /// @param pyMappedClass Mapped Python class. The user type steals the
        /// reference. Can be NULL, in which case the tuple type cannot be
        /// NULL.
        /// @param pyTupleType Python tuple type. The user type steals the
        /// reference. Must not be NULL if the mapped class is NULL.
        /// @param subtypes Subtypes. The tuple type takes over ownership of
        /// the references, and they should therefore *not* be released by
        /// the caller. This is not enforced, so be wary.
        CqlUserType(const NamesAndTypeVector& namesAndTypes,
                    PyObject* pyMappedClass,
                    PyObject* pyTupleType);


        ~CqlUserType();


        virtual PyObject* Deserialize(Buffer& buffer,
                                      int protocolVersion);
    private:
        virtual PyObject* DeserializeToMappedClass(Buffer& buffer,
                                                   int protocolVersion);


        virtual PyObject* DeserializeToTuple(Buffer& buffer,
                                             int protocolVersion);


        NamesAndTypeVector _namesAndTypes;
        ScopedReference _pyMappedClass;
        ScopedReference _pyTupleType;
    };


#undef DECLARE_SIMPLE_CQL_TYPE_CLASS
}
#endif
