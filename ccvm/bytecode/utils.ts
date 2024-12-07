
export type Dict<T> = { [key: string]: T };

export function error(message: string) {
    console.error('ERROR:', message);
}

export function warning(message: string) {
    console.warn('WARNING:', message);
}

export function newDict<T>() {
    return Object.create(null) as Dict<T>;
}

export function assertUnreachable(x: never): never {
    throw new Error("Didn't expect to get here");
}

export function replaceObjectContent<T extends {[key: string]: any}>(target: T, source: T): void {
    Object.keys(target).forEach(key => {
        delete target[key];
    });
    Object.keys(source).forEach(key => {
        (target as any)[key] = source[key];
    });
}
