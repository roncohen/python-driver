#include <arpa/inet.h>
#include <iostream>

#include "cql_types.hpp"
#include "cql_type_factory.hpp"
#include "marshal.hpp"


using namespace pyccassandra;


/// Resolve the subtypes of a CQL type.

/// @param pyCqlType Python representation of the CQL type.
/// @param factory CQL type factory.
/// @param subtypes Reference to vector to hold subtypes on success.
/// @returns true if the subtypes were successfully resolved, otherwise false,
/// indicating failure with the corresponding Python exception set.
bool ResolveSubtypes(PyObject* pyCqlType,
                     CqlTypeFactory& factory,
                     std::vector<CqlTypeReference*>& subtypes)
{
    // Attempt to resolve the subtypes.
    PyObject* pySubtypeList = PyObject_GetAttrString(pyCqlType, "subtypes");
    if (!pySubtypeList)
        return false;

    // Resolve Python representations of subtypes.
    std::vector<PyObject*> pySubtypes;

    if (PyTuple_Check(pySubtypeList))
    {
        Py_ssize_t numSubtypes = PyTuple_Size(pySubtypeList);
        pySubtypes.reserve(numSubtypes);

        for (Py_ssize_t i = 0; i < numSubtypes; ++i)
        {
            PyObject* pySubtype = PyTuple_GetItem(pySubtypeList, i);
            if (pySubtype == NULL)
            {
                Py_DECREF(pySubtypeList);
                return false;
            }

            pySubtypes.push_back(pySubtype);
        }
    }
    else if (PyList_Check(pySubtypeList))
    {
        Py_ssize_t numSubtypes = PyList_Size(pySubtypeList);
        pySubtypes.reserve(numSubtypes);

        for (Py_ssize_t i = 0; i < numSubtypes; ++i)
        {
            PyObject* pySubtype = PyList_GetItem(pySubtypeList, i);
            if (pySubtype == NULL)
            {
                Py_DECREF(pySubtypeList);
                return false;
            }

            pySubtypes.push_back(pySubtype);
        }
    }
    else
    {
        Py_DECREF(pySubtypeList);
        PyErr_SetString(PyExc_TypeError, "invalid subtypes for tuple");
        return false;
    }

    // Resolve the subtypes.
    for (std::size_t i = 0; i < pySubtypes.size(); ++i)
    {
        PyObject* pySubtype = pySubtypes[i];
        CqlTypeReference* subtype = factory.ReferenceFromPython(pySubtype);

        if (!subtype)
        {
            for (std::size_t j = 0; j < subtypes.size(); ++j)
                delete subtypes[j];
            subtypes.clear();

            Py_DECREF(pySubtypeList);

            return false;
        }

        subtypes.push_back(subtype);
    }

    Py_DECREF(pySubtypeList);

    return true;
}


CqlType::~CqlType()
{

}


#define IMPLEMENT_SIMPLE_CQL_TYPE_DESERIALIZE(_cls, _desc, _size, _unmarshal) \
    PyObject* _cls::Deserialize(Buffer& buffer, int)                    \
    {                                                                   \
        const unsigned char* data = buffer.Consume(_size);              \
        if (!data)                                                      \
        {                                                               \
            PyErr_SetString(PyExc_EOFError,                             \
                            "unexpected end of buffer deserializing "   \
                            _desc);                                     \
            return NULL;                                                \
        }                                                               \
                                                                        \
        return _unmarshal(data);                                        \
    }


IMPLEMENT_SIMPLE_CQL_TYPE_DESERIALIZE(CqlInt32Type,
                                      "signed 32-bit integer",
                                      4,
                                      UnmarshalInt32)
IMPLEMENT_SIMPLE_CQL_TYPE_DESERIALIZE(CqlLongType,
                                      "signed 64-bit integer",
                                      8,
                                      UnmarshalInt64)
IMPLEMENT_SIMPLE_CQL_TYPE_DESERIALIZE(CqlFloatType,
                                      "32-bit floating point number",
                                      4,
                                      UnmarshalFloat32)
IMPLEMENT_SIMPLE_CQL_TYPE_DESERIALIZE(CqlDoubleType,
                                      "64-bit floating point number",
                                      8,
                                      UnmarshalFloat64)
IMPLEMENT_SIMPLE_CQL_TYPE_DESERIALIZE(CqlBooleanType,
                                      "boolean",
                                      1,
                                      UnmarshalBoolean)


#undef IMPLEMENT_SIMPLE_CQL_TYPE_DESERIALIZE


PyObject* CqlBytesType::Deserialize(Buffer& buffer, int)
{
    const std::size_t size = buffer.Residual();
    if (size == 0)
        return PyString_FromStringAndSize("", 0);

    return PyString_FromStringAndSize((const char*)(buffer.Consume(size)),
                                      Py_ssize_t(size));
}


PyObject* CqlUtf8Type::Deserialize(Buffer& buffer, int)
{
    const std::size_t size = buffer.Residual();
    const char* data = (size ?
                        (const char*)(buffer.Consume(size)) :
                        "");

    PyObject* str = PyString_FromStringAndSize(data, Py_ssize_t(size));
    if (!str)
        return NULL;

    PyObject* dec = PyString_AsDecodedObject(str, "utf-8", "strict");

    Py_DECREF(str);

    return dec;
}


PyObject* CqlUuidType::Deserialize(Buffer& buffer, int)
{
    const std::size_t size = buffer.Residual();
    const char* data = (size ?
                        (const char*)(buffer.Consume(size)) :
                        "");

    PyObject* str = PyString_FromStringAndSize(data, Py_ssize_t(size));
    if (!str)
        return NULL;

    PyObject* uuid = NULL;
    PyObject* args = PyTuple_Pack(2, Py_None, str);
    if (args)
    {
        uuid = PyObject_CallObject(_pythonUuidType, args);
        Py_DECREF(args);
    }

    Py_DECREF(str);

    return uuid;
}


PyObject* CqlInetAddressType::Deserialize(Buffer& buffer, int)
{
    const std::size_t size = buffer.Residual();

    if ((size != 4) && (size != 16))
    {
        PyErr_SetString(PyExc_ValueError,
                        "expected buffer to either represent a 4 or 16 octet "
                        "network address");
        return NULL;
    }

    const char* data = (size ? (const char*)(buffer.Consume(size)) : NULL);
    char presentation[INET6_ADDRSTRLEN];

    if (!inet_ntop(size == 4 ? AF_INET : AF_INET6,
                   data,
                   presentation,
                   INET6_ADDRSTRLEN))
    {
        PyErr_SetString(PyExc_OSError, "error converting Internet address");
        return NULL;
    }

    return PyString_FromString(presentation);
}


PyObject* CqlDateType::Deserialize(Buffer& buffer, int)
{
    const unsigned char* data = buffer.Consume(8);
    if (!data)
    {
        PyErr_SetString(PyExc_EOFError,
                        "unexpected end of buffer deserializing date");
        return NULL;
    }

    int64_t timestampMs = UnpackInt64(data);

    PyObject* pyTimestamp = PyFloat_FromDouble(double(timestampMs) / 1000.0);
    if (!pyTimestamp)
        return NULL;

    PyObject* date = NULL;
    PyObject* args = PyTuple_Pack(1, pyTimestamp);
    if (args)
    {
        date = PyObject_CallObject(_pythonDatetimeUtcFromTimestamp, args);
        Py_DECREF(args);
    }

    Py_DECREF(pyTimestamp);

    return date;
}


PyObject* CqlIntegerType::Deserialize(Buffer& buffer, int)
{
    const std::size_t size = buffer.Residual();
    return UnmarshalVarint(buffer.Consume(size), size);
}


PyObject* CqlDecimalType::Deserialize(Buffer& buffer, int)
{
    PyObject* result = NULL;
    
    // Deserialize the scale.
    const unsigned char* scaleData = buffer.Consume(4);
    if (!scaleData)
    {
        PyErr_SetString(PyExc_EOFError,
                        "unexpected end of buffer deserializing "
                        "decimal number");
        return NULL;
    }
    int32_t negativeScale = UnpackInt32(scaleData);

    PyObject* scale = PyInt_FromLong(-negativeScale);
    if (scale)
    {
        // Deserialize the unscaled value.
        const std::size_t size = buffer.Residual();
        PyObject* unscaled = UnmarshalVarint(buffer.Consume(size), size);
        if (unscaled)
        {
            // Format the string representation of the decimal number.
            PyObject* format = PyString_FromString("%de%d");
            if (format)
            {
                PyObject* formatArgs = PyTuple_Pack(2, unscaled, scale);
                if (formatArgs)
                {
                    PyObject* stringRepr = PyString_Format(format, formatArgs);
                    if (stringRepr)
                    {
                        PyObject* args = PyTuple_Pack(1, stringRepr);
                        if (args)
                        {
                            result = PyObject_CallObject(_pythonDecimalType,
                                                         args);
                            Py_DECREF(args);
                        }

                        Py_DECREF(stringRepr);
                    }

                    Py_DECREF(formatArgs);
                }
                
                Py_DECREF(format);
            }

            Py_DECREF(unscaled);
        }

        Py_DECREF(scale);
    }

    return result;
}

CqlTupleType::CqlTupleType(const std::vector<CqlTypeReference*>& subtypes)
    :   _subtypes(subtypes)
{
    
}

CqlTupleType* CqlTupleType::FromPython(PyObject* pyCqlType,
                                       CqlTypeFactory& factory)
{
    std::vector<CqlTypeReference*> subtypes;
    if (!ResolveSubtypes(pyCqlType, factory, subtypes))
        return NULL;

    return new CqlTupleType(subtypes);
}

CqlTupleType::~CqlTupleType()
{
    std::vector<CqlTypeReference*>::iterator it = _subtypes.begin();

    while (it != _subtypes.end())
        delete *it++;
}

PyObject* CqlTupleType::Deserialize(Buffer& buffer, int protocolVersion)
{
    // Items in tuples are always encoded with at least protocol version 3.
    if (protocolVersion < 3)
        protocolVersion = 3;

    // Initialize a tuple.
    PyObject* tuple = PyTuple_New(_subtypes.size());
    if (!tuple)
        return NULL;

    // Drain as many items from the buffer as possible.
    std::size_t missing = _subtypes.size();

    for (std::size_t i = 0; i < _subtypes.size(); ++i)
    {
        // Read the size of the item.
        const unsigned char* sizeData = buffer.Consume(4);
        if (!sizeData)
            break;
        int32_t size = UnpackInt32(sizeData);

        // Create a local buffer for the item.
        if (size < 0)
        {
            Py_DECREF(tuple);
            PyErr_SetString(PyExc_ValueError, "negative item size in tuple");
            return NULL;
        }

        const unsigned char* itemData = buffer.Consume(size);
        if (!itemData)
        {
            Py_DECREF(tuple);
            PyErr_SetString(PyExc_EOFError,
                            "unexpected end of buffer while reading tuple");
            return NULL;
        }

        Buffer itemBuffer(itemData, size);
        PyObject* des = _subtypes[i]->Get()->Deserialize(itemBuffer,
                                                         protocolVersion);
        if (!des)
        {
            Py_DECREF(tuple);
            return NULL;
        }

        PyTuple_SetItem(tuple, i, des);

        --missing;
    }

    // Backfill with Nones.
    while (missing--)
        PyTuple_SetItem(tuple, missing, Py_None);

    return tuple;
}

CqlListType::CqlListType(CqlTypeReference* itemType)
    :   _itemType(itemType)
{
    
}

CqlListType* CqlListType::FromPython(PyObject* pyCqlType,
                                     CqlTypeFactory& factory)
{
    // Attempt to resolve the subtypes.
    std::vector<CqlTypeReference*> subtypes;
    if (!ResolveSubtypes(pyCqlType, factory, subtypes))
        return NULL;

    // Make sure there's only one subtype.
    if (subtypes.size() != 1)
    {
        PyErr_SetString(PyExc_TypeError, "list does not have one subtype");
        return NULL;
    }

    return new CqlListType(subtypes[0]);
}

CqlListType::~CqlListType()
{
    delete _itemType;
}

PyObject* CqlListType::Deserialize(Buffer& buffer, int protocolVersion)
{
    // Determine the number of items in the list.
    std::size_t itemCount;
    std::size_t sizeSize = (protocolVersion >= 3 ? 4 : 2);

    const unsigned char* itemCountData = buffer.Consume(sizeSize);
    if (!itemCountData)
    {
        PyErr_SetString(PyExc_EOFError,
                        "unexpected end of buffer while reading list");
        return NULL;
    }

    itemCount = (protocolVersion >= 3 ?
                 UnpackInt32(itemCountData) :
                 UnpackInt16(itemCountData));

    // Initialize a list.
    PyObject* list = PyList_New(itemCount);
    if (!list)
        return NULL;

    // Drain the items from the list.
    for (std::size_t i = 0; i < itemCount; ++i)
    {
        // Read the size of the item.
        const unsigned char* sizeData = buffer.Consume(sizeSize);
        if (!sizeData)
            break;
        int32_t size = (protocolVersion >= 3 ?
                        UnpackInt32(sizeData) :
                        UnpackInt16(sizeData));

        // Create a local buffer for the item.
        if (size < 0)
        {
            Py_DECREF(list);
            PyErr_SetString(PyExc_ValueError, "negative item size in list");
            return NULL;
        }

        const unsigned char* itemData = buffer.Consume(size);
        if (!itemData)
        {
            Py_DECREF(list);
            PyErr_SetString(PyExc_EOFError,
                            "unexpected end of buffer while reading list");
            return NULL;
        }

        Buffer itemBuffer(itemData, size);
        PyObject* des = _itemType->Get()->Deserialize(itemBuffer,
                                                      protocolVersion);
        if (!des)
        {
            Py_DECREF(list);
            return NULL;
        }

        PyList_SetItem(list, i, des);
    }

    return list;
}