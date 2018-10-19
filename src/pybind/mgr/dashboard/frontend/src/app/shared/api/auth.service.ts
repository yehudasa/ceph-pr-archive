import { HttpClient } from '@angular/common/http';
import { Injectable } from '@angular/core';

import { Credentials } from '../models/credentials';
import { AuthStorageService } from '../services/auth-storage.service';
import { ApiModule } from './api.module';

@Injectable({
  providedIn: ApiModule
})
export class AuthService {
  constructor(private authStorageService: AuthStorageService, private http: HttpClient) {}

  login(credentials: Credentials) {
    return this.http
      .post('api/auth', credentials)
      .toPromise()
      .then((resp: Credentials) => {
        this.authStorageService.set(resp.username, resp.permissions);
      });
  }

  logout(callback: Function) {
    return this.http.delete('api/auth').subscribe(() => {
      this.authStorageService.remove();
      if (callback) {
        callback();
      }
    });
  }
}
