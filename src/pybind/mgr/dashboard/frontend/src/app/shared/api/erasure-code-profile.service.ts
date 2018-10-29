import { HttpClient } from '@angular/common/http';
import { Injectable } from '@angular/core';

import { Observable } from 'rxjs';

import { ErasureCodeProfile } from '../models/erasure-code-profile';
import { ApiModule } from './api.module';

@Injectable({
  providedIn: ApiModule
})
export class ErasureCodeProfileService {
  constructor(private http: HttpClient) {}

  list(): Observable<ErasureCodeProfile[]> {
    return this.http.get<ErasureCodeProfile[]>('api/erasure_code_profile');
  }
}
