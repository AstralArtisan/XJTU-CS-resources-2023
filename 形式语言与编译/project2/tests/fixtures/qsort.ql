{
int a[10];

void swap(int arr[]; int i; int j;) {
    int temp;
    temp = arr[i];
    arr[i] = arr[j];
    arr[j] = temp;
}

int partition(int arr[]; int low; int high;) {
    int pivot;
    int i;
    int j;
    pivot = arr[high];
    i = low - 1;
    j = low - 1;
    while ((j = j + 1) < high) {
        if (arr[j] < pivot) {
            i = i + 1;
            swap(arr[], i, j,);
        }
    }
    swap(arr[], i + 1, high,);
    return i + 1;
}

void qsort(int arr[]; int low; int high;) {
    int p;
    if (low < high) {
        p = partition(arr, low, high,);
        qsort(arr, low, p - 1,);
        qsort(arr, p + 1, high,);
    }
}

qsort(a[], 0, 9,);
}
