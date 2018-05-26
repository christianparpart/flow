## Flow

**Flow** is a domain specific language designed to define control flow for
customizable hooks in your application, such as for request routing, event routing, and
more.

This project provides you with the Flow language frontend, compiler backend, and a virtual machine.

### Main Features

- A deterministic, non-turing-complete domain specific language for routing and configuration
- Special purpose literal types, such as: IPv4/IPv6 address, CIDR network, regular expression.
- Extensibility through the module system
- Execution Quota, making sure your scripts terminate within a given amount of instructions.
- Configurable language features during script compilation
- ...

### Use Flow, if ...

Use Flow if:

- speed matters
- reliability of termination of your scripts matter.
- table based configuration is not enough

### Notes

Flow was initially designed to act as a highly flexible configuration language in the
[x0 HTTP web application server](https://github.com/christianparpart/x0) and then evolved
into a routing framework that can be used in many host application.
